#include <QMessageBox>
#include <QtDebug>
#include <QXmlStreamReader>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMenu>
#include <QAction>

#include "library/banshee/bansheedbconnection.h"
#include "library/banshee/bansheefeature.h"
#include "library/banshee/bansheeplaylistmodel.h"
#include "library/dao/settingsdao.h"


const QString BansheeFeature::BANSHEE_MOUNT_KEY = "mixxx.BansheeFeature.mount";
QString BansheeFeature::m_databaseFile;

BansheeFeature::BansheeFeature(QObject* parent, TrackCollection* pTrackCollection, ConfigObject<ConfigValue>* pConfig)
        : LibraryFeature(parent),
          m_pTrackCollection(pTrackCollection),
          m_cancelImport(false)
{
    m_databaseFile = pConfig->getValueString(ConfigKey("[Banshee]","Database"));
    if (!QFile::exists(m_databaseFile)) {
        // Fall back to default
        m_databaseFile = BansheeDbConnection::getDatabaseFile();
    }

    m_pBansheePlaylistModel = new BansheePlaylistModel(this, m_pTrackCollection);
    m_isActivated = false;
    m_title = tr("Banshee");

    connect(&m_future_watcher, SIGNAL(finished()), this, SLOT(onTrackCollectionLoaded()));

    m_pAddToAutoDJAction = new QAction(tr("Add to Auto DJ bottom"),this);
    connect(m_pAddToAutoDJAction, SIGNAL(triggered()),
            this, SLOT(slotAddToAutoDJ()));

    m_pAddToAutoDJTopAction = new QAction(tr("Add to Auto DJ top 2"),this);
    connect(m_pAddToAutoDJTopAction, SIGNAL(triggered()),
            this, SLOT(slotAddToAutoDJTop()));

    m_pImportAsMixxxPlaylistAction = new QAction(tr("Import as Mixxx Playlist"), this);
    connect(m_pImportAsMixxxPlaylistAction, SIGNAL(triggered()),
            this, SLOT(slotImportAsMixxxPlaylist()));
}

BansheeFeature::~BansheeFeature() {
    qDebug() << "~BansheeFeature()";
    // stop import thread, if still running
    m_cancelImport = true;
    if (m_future.isRunning()) {
        qDebug() << "m_future still running";
        m_future.waitForFinished();
        qDebug() << "m_future finished";
    }

    delete m_pBansheePlaylistModel;
    delete m_pAddToAutoDJAction;
    delete m_pAddToAutoDJTopAction;
    delete m_pImportAsMixxxPlaylistAction;
}

// static
bool BansheeFeature::isSupported() {
    return !m_databaseFile.isEmpty();
}

// static
void BansheeFeature::prepareDbPath(ConfigObject<ConfigValue>* pConfig) {
    m_databaseFile = pConfig->getValueString(ConfigKey("[Banshee]","Database"));
    if (!QFile::exists(m_databaseFile)) {
        // Fall back to default
        m_databaseFile = BansheeDbConnection::getDatabaseFile();
    }
}

QVariant BansheeFeature::title() {
    return m_title;
}

QIcon BansheeFeature::getIcon() {
    return QIcon(":/images/library/ic_library_banshee.png");
}

void BansheeFeature::activate() {
    activate(false);
}

void BansheeFeature::activate(bool forceReload) {
    //qDebug("BansheeFeature::activate()");

    if (!m_isActivated || forceReload) {

        SettingsDAO settings(m_pTrackCollection->getDatabase());

        // Besser aus der Textdatei Lesen
//         tmp_string = m_pConfig->getValueString(ConfigKey(SHOUTCAST_PREF_KEY,"stream_website"));

        QString dbSetting(settings.getValue(BANSHEE_MOUNT_KEY));
        // if a path exists in the database, use it
        if (!dbSetting.isEmpty() && QFile::exists(dbSetting)) {
            m_dbfile = dbSetting;
        } else {
            // No Path in settings
            m_dbfile = "";
        }

        m_dbfile = detectMountPoint(m_dbfile);

        if (!QFile::exists(m_dbfile)) {
            m_dbfile = QFileDialog::getExistingDirectory(
                NULL,
                tr("Select your Banshee mount"));
            if (m_dbfile.isEmpty()) {
                return;
            }
        }

        m_isActivated =  true;

        QThreadPool::globalInstance()->setMaxThreadCount(4); //Tobias decided to use 4
        // Let a worker thread do the Banshee parsing
        m_future = QtConcurrent::run(this, &BansheeFeature::importLibrary);
        m_future_watcher.setFuture(m_future);
        m_title = tr("(loading) Banshee"); // (loading) at start in respect to small displays
        //calls a slot in the sidebar model such that '(loading)Banshee' is displayed.
        emit (featureIsLoading(this));
    }
    else{
        //m_pBansheePlaylistModel->setPlaylist(itdb_playlist_mpl(m_itdb)); // Gets the master playlist
        emit(showTrackModel(m_pBansheePlaylistModel));
    }
}

QString BansheeFeature::detectMountPoint( QString BansheeMountPoint) {
    QFileInfoList mountpoints;
    #ifdef __WINDOWS__
      // Windows Banshee Detection
      mountpoints = QDir::drives();
    #elif __LINUX__
      // Linux
      mountpoints = QDir("/media").entryInfoList();
      mountpoints += QDir("/mnt").entryInfoList();
    #elif __OSX__
      // Mac OSX
      mountpoints = QDir("/Volumes").entryInfoList();
    #endif

    QListIterator<QFileInfo> i(mountpoints);
    QFileInfo mp;
    while (i.hasNext()) {
        mp = (QFileInfo) i.next();
        qDebug() << "mp:" << mp.filePath();
        if( mp.filePath() != BansheeMountPoint ) {
            if (QDir( QString(mp.filePath() + "/Banshee_Control") ).exists() ) {
                qDebug() << "Banshee found at" << mp.filePath();

                // Multiple Banshee
                if (!BansheeMountPoint.isEmpty()) {
                    int ret = QMessageBox::warning(NULL, tr("Multiple Banshee Detected"),
                           tr("Mixxx has detected another Banshee. \n\n")+
                           tr("Choose Yes to use the newly found Banshee @ ")+ mp.filePath()+
                           tr(" or to continue to search for other Banshee. \n")+
                           tr("Choose No to use the existing Banshee @ ")+ BansheeMountPoint +
                           tr( " and end detection. \n"),
                           QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                    if (ret == QMessageBox::No) {
                    break;
                    }
                }
                BansheeMountPoint = mp.filePath();
            }
        }
    }
    return BansheeMountPoint;
}

void BansheeFeature::activateChild(const QModelIndex& index) {
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    qDebug() << "BansheeFeature::activateChild " << item->data() << " " << item->dataPath();
    QString playlist = item->dataPath().toString();
 /*   Itdb_Playlist* pPlaylist = (Itdb_Playlist*)playlist.toUInt();

    if (pPlaylist) {
        qDebug() << "Activating " << QString::fromUtf8(pPlaylist->name);
    }
    m_pBansheePlaylistModel->setPlaylist(pPlaylist);
    emit(showTrackModel(m_pBansheePlaylistModel));
    */
}

TreeItemModel* BansheeFeature::getChildModel() {
    return &m_childModel;
}

void BansheeFeature::onRightClick(const QPoint& globalPos) {
    Q_UNUSED(globalPos);
}

void BansheeFeature::onRightClickChild(const QPoint& globalPos, QModelIndex index) {
    //Save the model index so we can get it in the action slots...
    m_lastRightClickedIndex = index;

    //Create the right-click menu
    QMenu menu(NULL);
    menu.addAction(m_pAddToAutoDJAction);
    menu.addAction(m_pAddToAutoDJTopAction);
    menu.addSeparator();
    menu.addAction(m_pImportAsMixxxPlaylistAction);
    menu.exec(globalPos);
}

bool BansheeFeature::dropAccept(QUrl url) {
    Q_UNUSED(url);
    return false;
}

bool BansheeFeature::dropAcceptChild(const QModelIndex& index, QUrl url) {
    Q_UNUSED(index);
    Q_UNUSED(url);
    return false;
}

bool BansheeFeature::dragMoveAccept(QUrl url) {
    Q_UNUSED(url);
    return false;
}

bool BansheeFeature::dragMoveAcceptChild(const QModelIndex& index, QUrl url) {
    Q_UNUSED(index);
    Q_UNUSED(url);
    return false;
}

/*
 * This method is executed in a separate thread
 * via QtConcurrent::run
 */
TreeItem* BansheeFeature::importLibrary() {
   // Give thread a low priority
    QThread* thisThread = QThread::currentThread();
    thisThread->setPriority(QThread::LowestPriority);

    qDebug() << "BansheeFeature::importLibrary() ";

    TreeItem* playlist_root = new TreeItem();
/*
    GError* err = 0;
    qDebug() << "Calling the libgpod db parser for:" << m_dbfile.toUtf8();
    m_itdb = itdb_parse(m_dbfile.toUtf8(), &err);

    if (err) {
        qDebug() << "There was an error, attempting to free db: "
                 << err->message;
        QMessageBox::warning(NULL, tr("Error Loading Banshee database"),
                err->message);
        g_error_free(err);
        if (m_itdb) {
            itdb_free(m_itdb);
            m_itdb = 0;
        }
    } else if (m_itdb) {
        GList* playlist_node;

        for (playlist_node = g_list_first(m_itdb->playlists);
             playlist_node != NULL;
             playlist_node = g_list_next(playlist_node))
        {
            Itdb_Playlist* pPlaylist;
            pPlaylist = (Itdb_Playlist*)playlist_node->data;
            if (!itdb_playlist_is_mpl(pPlaylist)) {
                QString playlistname = QString::fromUtf8(pPlaylist->name);
                qDebug() << playlistname;
                // append the playlist to the child model
                TreeItem *item = new TreeItem(playlistname, QString::number((uint)pPlaylist), this, playlist_root);
                playlist_root->appendChild(item);
            }
        }
    }
    */
    return playlist_root;
}

void BansheeFeature::onTrackCollectionLoaded(){
    TreeItem* root = m_future.result();
    if(root){
        m_childModel.setRootItem(root);
        if (m_isActivated) {
            activate();
        }
        qDebug() << "Banshee library loaded: success";
        SettingsDAO settings(m_pTrackCollection->getDatabase());
        settings.setValue(BANSHEE_MOUNT_KEY, m_dbfile);
    }
    else{
        QMessageBox::warning(
            NULL,
            tr("Error Loading Banshee database"),
            tr("There was an error loading your Banshee database. Some of "
               "your Banshee tracks or playlists may not have loaded."));
    }
    //calls a slot in the sidebarmodel such that 'isLoading' is removed from the feature title.
    m_title = tr("Banshee");
    emit(featureLoadingFinished(this));
    activate();
}

void BansheeFeature::onLazyChildExpandation(const QModelIndex &index){
    Q_UNUSED(index);
    //Nothing to do because the childmodel is not of lazy nature.
}

void BansheeFeature::slotAddToAutoDJ() {
    //qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();
    addToAutoDJ(false); // Top = True
}


void BansheeFeature::slotAddToAutoDJTop() {
    //qDebug() << "slotAddToAutoDJTop() row:" << m_lastRightClickedIndex.data();
    addToAutoDJ(true); // bTop = True
}

void BansheeFeature::addToAutoDJ(bool bTop) {
    // qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();

    if (m_lastRightClickedIndex.isValid()) {
        TreeItem *item = static_cast<TreeItem*>(m_lastRightClickedIndex.internalPointer());
        qDebug() << "BansheeFeature::addToAutoDJ " << item->data() << " " << item->dataPath();
        QString playlist = item->dataPath().toString();
 /*       Itdb_Playlist* pPlaylist = (Itdb_Playlist*)playlist.toUInt();
        if (pPlaylist) {
            BansheePlaylistModel* pPlaylistModelToAdd = new BansheePlaylistModel(this, m_pTrackCollection);
            pPlaylistModelToAdd->setPlaylist(pPlaylist);
            PlaylistDAO &playlistDao = m_pTrackCollection->getPlaylistDAO();
            int autoDJId = playlistDao.getPlaylistIdFromName(AUTODJ_TABLE);

            int rows = pPlaylistModelToAdd->rowCount();
            for(int i = 0; i < rows; ++i){
                QModelIndex index = pPlaylistModelToAdd->index(i,0);
                if (index.isValid()) {
                    TrackPointer track = pPlaylistModelToAdd->getTrack(index);
                    if (bTop) {
                        // Start at position 2 because position 1 was already loaded to the deck
                        playlistDao.insertTrackIntoPlaylist(track->getId(), autoDJId, i+2);
                    } else {
                        playlistDao.appendTrackToPlaylist(track->getId(), autoDJId);
                    }
                }
            }
            delete pPlaylistModelToAdd;
        }
        */
    }
}

void BansheeFeature::slotImportAsMixxxPlaylist() {
    // qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();

    if (m_lastRightClickedIndex.isValid()) {
        TreeItem *item = static_cast<TreeItem*>(m_lastRightClickedIndex.internalPointer());
        qDebug() << "BansheeFeature::slotImportAsMixxxPlaylist " << item->data() << " " << item->dataPath();
        QString playlist = item->dataPath().toString();
/*
        Itdb_Playlist* pPlaylist = (Itdb_Playlist*)playlist.toUInt();
        playlist = QString::fromUtf8(pPlaylist->name);
        if (pPlaylist) {
            BansheePlaylistModel* pPlaylistModelToAdd = new BansheePlaylistModel(this, m_pTrackCollection);
  //          pPlaylistModelToAdd->setPlaylist(pPlaylist);
            PlaylistDAO &playlistDao = m_pTrackCollection->getPlaylistDAO();

            int playlistId = playlistDao.getPlaylistIdFromName(playlist);
            int i = 1;

            if (playlistId != -1) {
                // Calculate a unique name
                playlist += "(%1)";
                while (playlistId != -1) {
                    i++;
                    playlistId = playlistDao.getPlaylistIdFromName(playlist.arg(i));
                }
                playlist = playlist.arg(i);
            }
            playlistId = playlistDao.createPlaylist(playlist);

            if (playlistId != -1) {
                // Copy Tracks
                int rows = pPlaylistModelToAdd->rowCount();
                for (int i = 0; i < rows; ++i) {
                    QModelIndex index = pPlaylistModelToAdd->index(i,0);
                    if (index.isValid()) {
                        //qDebug() << pPlaylistModelToAdd->getTrackLocation(index);
                        TrackPointer track = pPlaylistModelToAdd->getTrack(index);
                        playlistDao.appendTrackToPlaylist(track->getId(), playlistId);
                    }
                }
            }
            else {
                QMessageBox::warning(NULL,
                                     tr("Playlist Creation Failed"),
                                     tr("An unknown error occurred while creating playlist: ")
                                      + playlist);
            }

            delete pPlaylistModelToAdd;
        }
    */
    }
    emit(featureUpdated());
}
