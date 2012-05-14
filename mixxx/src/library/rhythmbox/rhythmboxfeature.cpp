#include <QMessageBox>
#include <QtDebug>
#include <QStringList>

#include "library/rhythmbox/rhythmboxtrackmodel.h"
#include "library/rhythmbox/rhythmboxplaylistmodel.h"
#include "library/rhythmbox/rhythmboxfeature.h"
#include "library/treeitem.h"
#include "library/queryutil.h"

RhythmboxFeature::RhythmboxFeature(QObject* parent, TrackCollection* pTrackCollection)
        : BaseExternalLibraryFeature(parent, pTrackCollection),
          m_pTrackCollection(pTrackCollection),
          m_cancelImport(false) {
    QString tableName = "rhythmbox_library";
    QString idColumn = "id";
    QStringList columns;
    columns << "id"
            << "artist"
            << "title"
            << "album"
            << "year"
            << "genre"
            << "tracknumber"
            << "location"
            << "comment"
            << "rating"
            << "duration"
            << "bitrate"
            << "bpm";
    pTrackCollection->addTrackSource(QString("rhythmbox"), QSharedPointer<BaseTrackCache>(
        new BaseTrackCache(m_pTrackCollection, tableName, idColumn,
                           columns, false)));

    m_pRhythmboxTrackModel = new RhythmboxTrackModel(this, m_pTrackCollection);
    m_pRhythmboxPlaylistModel = new RhythmboxPlaylistModel(this, m_pTrackCollection);
    m_isActivated =  false;
    m_title = tr("Rhythmbox");

    m_database = QSqlDatabase::cloneDatabase( pTrackCollection->getDatabase(), "RHYTHMBOX_SCANNER");

    //Open the database connection in this thread.
    if (!m_database.open()) {
        qDebug() << "Failed to open database for Rhythmbox scanner." << m_database.lastError();
    }
    connect(&m_track_watcher, SIGNAL(finished()),
            this, SLOT(onTrackCollectionLoaded()),
            Qt::QueuedConnection);

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

RhythmboxFeature::~RhythmboxFeature() {
    m_database.close();
    // stop import thread, if still running
    m_cancelImport = true;
    m_track_future.waitForFinished();
    delete m_pRhythmboxTrackModel;
    delete m_pRhythmboxPlaylistModel;
    delete m_pAddToAutoDJAction;
    delete m_pAddToAutoDJTopAction;
    delete m_pImportAsMixxxPlaylistAction;
}

BaseSqlTableModel* RhythmboxFeature::getPlaylistModelForPlaylist(QString playlist) {
    RhythmboxPlaylistModel* pModel = new RhythmboxPlaylistModel(this, m_pTrackCollection);
    pModel->setPlaylist(playlist);
    return pModel;
}

bool RhythmboxFeature::isSupported() {
    return (QFile::exists(QDir::homePath() + "/.gnome2/rhythmbox/rhythmdb.xml") ||
            QFile::exists(QDir::homePath() + "/.local/share/rhythmbox/rhythmdb.xml"));
}

QVariant RhythmboxFeature::title() {
    return m_title;
}

QIcon RhythmboxFeature::getIcon() {
    return QIcon(":/images/library/ic_library_rhythmbox.png");
}

TreeItemModel* RhythmboxFeature::getChildModel() {
    return &m_childModel;
}

void RhythmboxFeature::activate() {
     qDebug() << "RhythmboxFeature::activate()";

    if(!m_isActivated){
        m_isActivated =  true;
        /* Ususally the maximum number of threads
         * is > 2 depending on the CPU cores
         * Unfortunately, within VirtualBox
         * the maximum number of allowed threads
         * is 1 at all times We'll need to increase
         * the number to > 1, otherwise importing the music collection
         * takes place when the GUI threads terminates, i.e., on
         * Mixxx shutdown.
         */
        QThreadPool::globalInstance()->setMaxThreadCount(4); //Tobias decided to use 4
        m_track_future = QtConcurrent::run(this, &RhythmboxFeature::importMusicCollection);
        m_track_watcher.setFuture(m_track_future);
        m_title = "(loading) Rhythmbox";
        //calls a slot in the sidebar model such that 'Rhythmbox (isLoading)' is displayed.
        emit (featureIsLoading(this));
    }

    emit(showTrackModel(m_pRhythmboxTrackModel));
}

void RhythmboxFeature::activateChild(const QModelIndex& index) {
    //qDebug() << "RhythmboxFeature::activateChild()" << index;
    QString playlist = index.data().toString();
    qDebug() << "Activating " << playlist;
    m_pRhythmboxPlaylistModel->setPlaylist(playlist);
    emit(showTrackModel(m_pRhythmboxPlaylistModel));
}

void RhythmboxFeature::onRightClickChild(const QPoint& globalPos, QModelIndex index) {
    //Save the model index so we can get it in the action slots...
    m_lastRightClickedIndex = index;

    //Create the right-click menu
    QMenu menu;
    menu.addAction(m_pAddToAutoDJAction);
    menu.addAction(m_pAddToAutoDJTopAction);
    menu.addSeparator();
    menu.addAction(m_pImportAsMixxxPlaylistAction);
    menu.exec(globalPos);
}

bool RhythmboxFeature::dropAccept(QUrl url) {
    Q_UNUSED(url);
    return false;
}

bool RhythmboxFeature::dropAcceptChild(const QModelIndex& index, QUrl url) {
    Q_UNUSED(index);
    Q_UNUSED(url);
    return false;
}

bool RhythmboxFeature::dragMoveAccept(QUrl url) {
    Q_UNUSED(url);
    return false;
}

bool RhythmboxFeature::dragMoveAcceptChild(const QModelIndex& index, QUrl url) {
    Q_UNUSED(index);
    Q_UNUSED(url);
    return false;
}

TreeItem* RhythmboxFeature::importMusicCollection()
{
    qDebug() << "importMusicCollection Thread Id: " << QThread::currentThread();
    /*
     * Try and open the Rhythmbox DB. An API call which tells us where
     * the file is would be nice.
     */
    QFile db(QDir::homePath() + "/.gnome2/rhythmbox/rhythmdb.xml");
    if ( ! db.exists()) {
        db.setFileName(QDir::homePath() + "/.local/share/rhythmbox/rhythmdb.xml");
        if ( ! db.exists())
            return false;
    }

    if (!db.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    //Delete all table entries of Traktor feature
    ScopedTransaction transaction(m_database);
    clearTable("rhythmbox_playlist_tracks");
    clearTable("rhythmbox_library");
    clearTable("rhythmbox_playlists");
    transaction.commit();

    transaction.transaction();
    QSqlQuery query(m_database);
    query.prepare("INSERT INTO rhythmbox_library (artist, title, album, year, genre, comment, tracknumber,"
                  "bpm, bitrate,"
                  "duration, location,"
                  "rating ) "
                  "VALUES (:artist, :title, :album, :year, :genre, :comment, :tracknumber,"
                  ":bpm, :bitrate,"
                  ":duration, :location, :rating )");


    QXmlStreamReader xml(&db);
    while (!xml.atEnd() && !m_cancelImport) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == "entry") {
            QXmlStreamAttributes attr = xml.attributes();
            //Check if we really parse a track and not album art information
            if(attr.value("type").toString() == "song"){
                importTrack(xml, query);
            }
        }
    }
    transaction.commit();

    if (xml.hasError()) {
        // do error handling
        qDebug() << "Cannot process Rhythmbox music collection";
        qDebug() << "XML ERROR: " << xml.errorString();
        return false;
    }

    db.close();
    if (m_cancelImport) {
        return NULL;
    }
    return importPlaylists();
}

TreeItem* RhythmboxFeature::importPlaylists()
{
    QFile db(QDir::homePath() + "/.gnome2/rhythmbox/playlists.xml");
    if ( ! db.exists()) {
        db.setFileName(QDir::homePath() + "/.local/share/rhythmbox/playlists.xml");
        if (!db.exists())
            return NULL;
    }
    //Open file
     if (!db.open(QIODevice::ReadOnly | QIODevice::Text))
        return NULL;

    QSqlQuery query_insert_to_playlists(m_database);
    query_insert_to_playlists.prepare("INSERT INTO rhythmbox_playlists (id, name) "
                                      "VALUES (:id, :name)");

    QSqlQuery query_insert_to_playlist_tracks(m_database);
    query_insert_to_playlist_tracks.prepare(
        "INSERT INTO rhythmbox_playlist_tracks (playlist_id, track_id, position) "
        "VALUES (:playlist_id, :track_id, :position)");
    //The tree structure holding the playlists
    TreeItem* rootItem = new TreeItem();

    QXmlStreamReader xml(&db);
    while (!xml.atEnd() && !m_cancelImport) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == "playlist") {
            QXmlStreamAttributes attr = xml.attributes();

            //Only parse non build-in playlists
            if(attr.value("type").toString() == "static"){
                QString playlist_name = attr.value("name").toString();

                //Construct the childmodel
                TreeItem * item = new TreeItem(playlist_name,playlist_name, this, rootItem);
                rootItem->appendChild(item);

                //Execute SQL statement
                query_insert_to_playlists.bindValue(":name", playlist_name);

                bool success = query_insert_to_playlists.exec();
                if(!success){
                    qDebug() << "SQL Error in RhythmboxFeature.cpp: line" << __LINE__ << " "
                                    << query_insert_to_playlists.lastError();
                    return NULL;
                }
                //get playlist_id
                int playlist_id = query_insert_to_playlists.lastInsertId().toInt();

                //Process playlist entries
                importPlaylist(xml, query_insert_to_playlist_tracks, playlist_id);

            }
        }
    }

    if (xml.hasError()) {
        // do error handling
        qDebug() << "Cannot process Rhythmbox music collection";
        qDebug() << "XML ERROR: " << xml.errorString();
        return NULL;
    }
    db.close();

    return rootItem;

}

void RhythmboxFeature::importTrack(QXmlStreamReader &xml, QSqlQuery &query)
{
    QString title;
    QString artist;
    QString album;
    QString year;
    QString genre;
    QString location;
    QUrl locationUrl;

    int bpm = 0;
    int bitrate = 0;

    //duration of a track
    int playtime = 0;
    int rating = 0;
    QString comment;
    QString tracknumber;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if(xml.name() == "title"){
                title = xml.readElementText();
                continue;
            }
            if(xml.name() == "artist"){
                artist = xml.readElementText();
                continue;
            }
            if(xml.name() == "genre"){
                genre = xml.readElementText();
                continue;
            }
            if(xml.name() == "album"){
                album = xml.readElementText();
                continue;
            }
            if(xml.name() == "track-number"){
                tracknumber = xml.readElementText();
                continue;
            }
            if(xml.name() == "duration"){
                playtime = xml.readElementText().toInt();;
                continue;
            }
            if(xml.name() == "bitrate"){
                bitrate = xml.readElementText().toInt();
                continue;
            }
            if(xml.name() == "beats-per-minute"){
                bpm = xml.readElementText().toInt();
                continue;
            }
            if(xml.name() == "comment"){
                comment = xml.readElementText();
                continue;
            }
            if(xml.name() == "location"){
                locationUrl = QUrl::fromEncoded( xml.readElementText().toUtf8() );
                continue;
            }
        }
        //exit the loop if we reach the closing <entry> tag
        if (xml.isEndElement() && xml.name() == "entry") {
            break;
        }
    }

    location = locationUrl.toLocalFile();

    if (location.isEmpty()) {
        // here in case of smb:// location
        // TODO(XXX) QUrl does not support SMB:// locations does Mixxx?
        // use ~/.gvfs location instead

        // Try to convert a smb path location
        QString temp_location = locationUrl.toString();

        if (temp_location.startsWith("smb://")) {
            // Hack for samba mounts works only on German GNOME Linux
            // TODO(xxx): use gio instead

            location = QDir::homePath() + "/.gvfs/";
            location += temp_location.section('/', 3, 3);
            location += " ";
            //location += tr("on", "gvfs folder separator" );
            location += "auf";
            location += " ";
            location += temp_location.section('/', 2, 2);
            location += "/";
            location += temp_location.section('/', 4);
        } else {
            return;
        }
    }

    query.bindValue(":artist", artist);
    query.bindValue(":title", title);
    query.bindValue(":album", album);
    query.bindValue(":genre", genre);
    query.bindValue(":year", year);
    query.bindValue(":duration", playtime);
    query.bindValue(":location", location);
    query.bindValue(":rating", rating);
    query.bindValue(":comment", comment);
    query.bindValue(":tracknumber", tracknumber);
    query.bindValue(":bpm", bpm);
    query.bindValue(":bitrate", bitrate);

    bool success = query.exec();

    if (!success) {
        qDebug() << "SQL Error in rhythmboxfeature.cpp: line" << __LINE__ << " " << query.lastError();
        return;
    }
}

/** reads all playlist entries and executes a SQL statement **/
void RhythmboxFeature::importPlaylist(QXmlStreamReader &xml, QSqlQuery &query_insert_to_playlist_tracks, int playlist_id)
{
    int playlist_position = 1;
    while(!xml.atEnd())
    {
        //read next XML element
        xml.readNext();
        if(xml.isStartElement() && xml.name() == "location")
        {
            QString location = xml.readElementText();
            location.remove("file://");
            QByteArray strlocbytes = location.toUtf8();
            QUrl locationUrl = QUrl::fromEncoded(strlocbytes);
            location = locationUrl.toLocalFile();

            //get the ID of the file in the rhythmbox_library table
            int track_id = -1;
            QSqlQuery finder_query(m_database);
            finder_query.prepare("select id from rhythmbox_library where location=:path");
            finder_query.bindValue(":path", location);
            bool success = finder_query.exec();


            if(success){
                while (finder_query.next()) {
                    track_id = finder_query.value(finder_query.record().indexOf("id")).toInt();
                }
             }
             else
                qDebug() << "SQL Error in RhythmboxFeature.cpp: line" << __LINE__ << " " << finder_query.lastError();

            query_insert_to_playlist_tracks.bindValue(":playlist_id", playlist_id);
            query_insert_to_playlist_tracks.bindValue(":track_id", track_id);
            query_insert_to_playlist_tracks.bindValue(":position", playlist_position++);
            success = query_insert_to_playlist_tracks.exec();

            if(!success){
                qDebug() << "SQL Error in RhythmboxFeature.cpp: line" << __LINE__ << " "
                             << query_insert_to_playlist_tracks.lastError();
                qDebug() << "trackid" << track_id;
                qDebug() << "playlis ID " << playlist_id;
                qDebug() << "-----------------";
            }
        }
        // Exit the the loop if we reach the closing <playlist> tag
        if (xml.isEndElement() && xml.name() == "playlist") {
            break;
        }
    }
}

void RhythmboxFeature::clearTable(QString table_name)
{
    qDebug() << "clearTable Thread Id: " << QThread::currentThread();
    QSqlQuery query(m_database);
    query.prepare("delete from "+table_name);
    bool success = query.exec();

    if (!success) {
        qDebug() << "Could not delete remove old entries from table "
                 << table_name << " : " << query.lastError();
    } else {
        qDebug() << "Rhythmbox table entries of '" << table_name
                 << "' have been cleared.";
    }
}

void RhythmboxFeature::onTrackCollectionLoaded() {
    TreeItem* root = m_track_future.result();
    if (root) {
        m_childModel.setRootItem(root);

        // Tell the rhythmbox track source that it should re-build its index.
        m_pTrackCollection->getTrackSource("rhythmbox")->buildIndex();

        //m_pRhythmboxTrackModel->select();
    } else {
         qDebug() << "Rhythmbox Playlists loaded: false";
    }

    // calls a slot in the sidebarmodel such that 'isLoading' is removed from
    // the feature title.
    m_title = tr("Rhythmbox");
    emit(featureLoadingFinished(this));
    activate();
}

void RhythmboxFeature::onLazyChildExpandation(const QModelIndex &index){
    //Nothing to do because the childmodel is not of lazy nature.
    Q_UNUSED(index);
}

void RhythmboxFeature::slotAddToAutoDJ() {
    //qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();
    addToAutoDJ(false); // Top = True
}


void RhythmboxFeature::slotAddToAutoDJTop() {
    //qDebug() << "slotAddToAutoDJTop() row:" << m_lastRightClickedIndex.data();
    addToAutoDJ(true); // bTop = True
}

void RhythmboxFeature::addToAutoDJ(bool bTop) {
    // qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();

    if (m_lastRightClickedIndex.isValid()) {

        QString playlist = m_lastRightClickedIndex.data().toString();
        RhythmboxPlaylistModel* pPlaylistModelToAdd = new RhythmboxPlaylistModel(this, m_pTrackCollection);
        pPlaylistModelToAdd->setPlaylist(playlist);
        pPlaylistModelToAdd->select();
        PlaylistDAO &playlistDao = m_pTrackCollection->getPlaylistDAO();
        int autoDJId = playlistDao.getPlaylistIdFromName(AUTODJ_TABLE);

        int rows = pPlaylistModelToAdd->rowCount();
        for(int i = 0; i < rows; ++i){
            QModelIndex index = pPlaylistModelToAdd->index(i,0);
            if (index.isValid()) {
                qDebug() << pPlaylistModelToAdd->getTrackLocation(index);
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
}

void RhythmboxFeature::slotImportAsMixxxPlaylist() {
    // qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();

    if (m_lastRightClickedIndex.isValid()) {

        QString playlist = m_lastRightClickedIndex.data().toString();
        RhythmboxPlaylistModel* pPlaylistModelToAdd = new RhythmboxPlaylistModel(this, m_pTrackCollection);
        pPlaylistModelToAdd->setPlaylist(playlist);
        pPlaylistModelToAdd->select();
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
                    qDebug() << pPlaylistModelToAdd->getTrackLocation(index);
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
}
