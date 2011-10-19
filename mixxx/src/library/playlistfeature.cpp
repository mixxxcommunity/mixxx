#include <QtDebug>
#include <QMenu>
#include <QInputDialog>
#include <QFileDialog>
#include <QDesktopServices>

#include "library/playlistfeature.h"
#include "library/parser.h"
#include "library/parserm3u.h"
#include "library/parserpls.h"


#include "widget/wlibrary.h"
#include "widget/wlibrarysidebar.h"
#include "widget/wlibrarytextbrowser.h"
#include "library/trackcollection.h"
#include "library/playlisttablemodel.h"
#include "mixxxkeyboard.h"
#include "treeitem.h"
#include "soundsourceproxy.h"

PlaylistFeature::PlaylistFeature(QObject* parent, TrackCollection* pTrackCollection, ConfigObject<ConfigValue>* pConfig)
        : LibraryFeature(parent),
         // m_pTrackCollection(pTrackCollection),
          m_playlistDao(pTrackCollection->getPlaylistDAO()),
          m_trackDao(pTrackCollection->getTrackDAO()),
          m_pConfig(pConfig),
          m_playlistTableModel(this, pTrackCollection->getDatabase()) {
    m_pPlaylistTableModel = new PlaylistTableModel(this, pTrackCollection);

    m_pCreatePlaylistAction = new QAction(tr("New Playlist"),this);
    connect(m_pCreatePlaylistAction, SIGNAL(triggered()),
            this, SLOT(slotCreatePlaylist()));

    m_pAddToAutoDJAction = new QAction(tr("Add to Auto-DJ Queue"),this);
    connect(m_pAddToAutoDJAction, SIGNAL(triggered()),
            this, SLOT(slotAddToAutoDJ()));

    m_pDeletePlaylistAction = new QAction(tr("Remove"),this);
    connect(m_pDeletePlaylistAction, SIGNAL(triggered()),
            this, SLOT(slotDeletePlaylist()));

    m_pRenamePlaylistAction = new QAction(tr("Rename"),this);
    connect(m_pRenamePlaylistAction, SIGNAL(triggered()),
            this, SLOT(slotRenamePlaylist()));

    m_pLockPlaylistAction = new QAction(tr("Lock"),this);
    connect(m_pLockPlaylistAction, SIGNAL(triggered()),
            this, SLOT(slotTogglePlaylistLock()));

    m_pImportPlaylistAction = new QAction(tr("Import Playlist"),this);
    connect(m_pImportPlaylistAction, SIGNAL(triggered()),
            this, SLOT(slotImportPlaylist()));
    m_pExportPlaylistAction = new QAction(tr("Export Playlist"), this);
    connect(m_pExportPlaylistAction, SIGNAL(triggered()),
            this, SLOT(slotExportPlaylist()));

    // Setup the sidebar playlist model
    m_playlistTableModel.setTable("Playlists");
    m_playlistTableModel.setFilter("hidden=0");
    m_playlistTableModel.setSort(m_playlistTableModel.fieldIndex("name"),
                                 Qt::AscendingOrder);
    m_playlistTableModel.select();

    //construct child model
    TreeItem *rootItem = new TreeItem();
    m_childModel.setRootItem(rootItem);
    constructChildModel();
}

PlaylistFeature::~PlaylistFeature() {
    delete m_pPlaylistTableModel;
    delete m_pCreatePlaylistAction;
    delete m_pDeletePlaylistAction;
    delete m_pImportPlaylistAction;
    delete m_pAddToAutoDJAction;
    delete m_pRenamePlaylistAction;
    delete m_pLockPlaylistAction;
}

QVariant PlaylistFeature::title() {
    return tr("Playlists");
}

QIcon PlaylistFeature::getIcon() {
    return QIcon(":/images/library/ic_library_playlist.png");
}


void PlaylistFeature::bindWidget(WLibrarySidebar* sidebarWidget,
                                 WLibrary* libraryWidget,
                                 MixxxKeyboard* keyboard) {
    WLibraryTextBrowser* edit = new WLibraryTextBrowser(libraryWidget);
    connect(this, SIGNAL(showPage(const QUrl&)),
            edit, SLOT(setSource(const QUrl&)));
    libraryWidget->registerView("PLAYLISTHOME", edit);
}

void PlaylistFeature::activate() {
    emit(showPage(QUrl("qrc:/html/playlists.html")));
    emit(switchToView("PLAYLISTHOME"));
}

void PlaylistFeature::activateChild(const QModelIndex& index) {
    //qDebug() << "PlaylistFeature::activateChild()" << index;

    //Switch the playlist table model's playlist.
    QString playlistName = index.data().toString();
    int playlistId = m_playlistDao.getPlaylistIdFromName(playlistName);
    m_pPlaylistTableModel->setPlaylist(playlistId);
    emit(showTrackModel(m_pPlaylistTableModel));
}

void PlaylistFeature::onRightClick(const QPoint& globalPos) {
    m_lastRightClickedIndex = QModelIndex();

    //Create the right-click menu
    QMenu menu(NULL);
    menu.addAction(m_pCreatePlaylistAction);
    menu.exec(globalPos);
}

void PlaylistFeature::onRightClickChild(const QPoint& globalPos, QModelIndex index) {
    //Save the model index so we can get it in the action slots...
    m_lastRightClickedIndex = index;
    QString playlistName = index.data().toString();
    int playlistId = m_playlistDao.getPlaylistIdFromName(playlistName);


    bool locked = m_playlistDao.isPlaylistLocked(playlistId);
    m_pDeletePlaylistAction->setEnabled(!locked);
    m_pRenamePlaylistAction->setEnabled(!locked);

    m_pLockPlaylistAction->setText(locked ? tr("Unlock") : tr("Lock"));


    //Create the right-click menu
    QMenu menu(NULL);
    menu.addAction(m_pCreatePlaylistAction);
    menu.addSeparator();
    menu.addAction(m_pAddToAutoDJAction);
    menu.addAction(m_pRenamePlaylistAction);
    menu.addAction(m_pDeletePlaylistAction);
    menu.addAction(m_pLockPlaylistAction);
    menu.addSeparator();
    menu.addAction(m_pImportPlaylistAction);
    menu.addAction(m_pExportPlaylistAction);
    menu.exec(globalPos);
}

void PlaylistFeature::slotCreatePlaylist() {
    QString name;
    bool validNameGiven = false;

    do {
        bool ok = false;
        name = QInputDialog::getText(NULL,
                                     tr("New Playlist"),
                                     tr("Playlist name:"),
                                     QLineEdit::Normal,
                                     tr("New Playlist"),
                                     &ok).trimmed();

        if (!ok)
            return;

        int existingId = m_playlistDao.getPlaylistIdFromName(name);

        if (existingId != -1) {
            QMessageBox::warning(NULL,
                                 tr("Playlist Creation Failed"),
                                 tr("A playlist by that name already exists."));
        }
        else if (name.isEmpty()) {
            QMessageBox::warning(NULL,
                                 tr("Playlist Creation Failed"),
                                 tr("A playlist cannot have a blank name."));
        }
        else {
            validNameGiven = true;
        }

    } while (!validNameGiven);

    bool playlistCreated = m_playlistDao.createPlaylist(name);

    if (playlistCreated) {
        clearChildModel();
        m_playlistTableModel.select();
        constructChildModel();
        emit(featureUpdated());
        //Switch the view to the new playlist.
        int playlistId = m_playlistDao.getPlaylistIdFromName(name);
        m_pPlaylistTableModel->setPlaylist(playlistId);
        // TODO(XXX) set sidebar selection
        emit(showTrackModel(m_pPlaylistTableModel));
    }
    else {
        QMessageBox::warning(NULL,
                             tr("Playlist Creation Failed"),
                             tr("An unknown error occurred while creating playlist: ")
                              + name);
    }
}

void PlaylistFeature::slotRenamePlaylist()
{
    qDebug() << "slotRenamePlaylist()";

    QString oldName = m_lastRightClickedIndex.data().toString();
    int playlistId = m_playlistDao.getPlaylistIdFromName(oldName);
    bool locked = m_playlistDao.isPlaylistLocked(playlistId);

    if (locked) {
        qDebug() << "Skipping playlist rename because playlist" << playlistId << "is locked.";
        return;
    }

    QString newName;
    bool validNameGiven = false;

    do {
        bool ok = false;
        newName = QInputDialog::getText(NULL,
                                        tr("Rename Playlist"),
                                        tr("New playlist name:"),
                                        QLineEdit::Normal,
                                        oldName,
                                        &ok).trimmed();

        if (!ok || oldName == newName) {
            return;
        }

        int existingId = m_playlistDao.getPlaylistIdFromName(newName);

        if (existingId != -1) {
            QMessageBox::warning(NULL,
                                tr("Renaming Playlist Failed"),
                                tr("A playlist by that name already exists."));
        }
        else if (newName.isEmpty()) {
            QMessageBox::warning(NULL,
                                tr("Renaming Playlist Failed"),
                                tr("A playlist cannot have a blank name."));
        }
        else {
            validNameGiven = true;
        }
    } while (!validNameGiven);

    m_playlistDao.renamePlaylist(playlistId, newName);
    clearChildModel();
    m_playlistTableModel.select();
    constructChildModel();
    emit(featureUpdated());
    m_pPlaylistTableModel->setPlaylist(playlistId);
}


void PlaylistFeature::slotTogglePlaylistLock()
{
    QString playlistName = m_lastRightClickedIndex.data().toString();
    int playlistId = m_playlistDao.getPlaylistIdFromName(playlistName);
    bool locked = !m_playlistDao.isPlaylistLocked(playlistId);

    if (!m_playlistDao.setPlaylistLocked(playlistId, locked)) {
        qDebug() << "Failed to toggle lock of playlistId " << playlistId;
    }

    TreeItem* playlistItem = m_childModel.getItem(m_lastRightClickedIndex);
    playlistItem->setIcon(locked ? QIcon(":/images/library/ic_library_locked.png") : QIcon());
}

void PlaylistFeature::slotDeletePlaylist()
{
    //qDebug() << "slotDeletePlaylist() row:" << m_lastRightClickedIndex.data();
    int playlistId = m_playlistDao.getPlaylistIdFromName(m_lastRightClickedIndex.data().toString());
    bool locked = m_playlistDao.isPlaylistLocked(playlistId);

    if (locked) {
        qDebug() << "Skipping playlist deletion because playlist" << playlistId << "is locked.";
        return;
    }

    if (m_lastRightClickedIndex.isValid() &&
        !m_playlistDao.isPlaylistLocked(playlistId)) {
        Q_ASSERT(playlistId >= 0);

        clearChildModel();
        m_playlistDao.deletePlaylist(playlistId);
        m_playlistTableModel.select();
        constructChildModel();
        emit(featureUpdated());
    }

}

bool PlaylistFeature::dropAccept(QUrl url) {
    return false;
}

bool PlaylistFeature::dropAcceptChild(const QModelIndex& index, QUrl url) {
    //TODO: Filter by supported formats regex and reject anything that doesn't match.
    QString playlistName = index.data().toString();
    int playlistId = m_playlistDao.getPlaylistIdFromName(playlistName);
    //m_playlistDao.appendTrackToPlaylist(url.toLocalFile(), playlistId);

    // If a track is dropped onto a playlist's name, but the track isn't in the
    // library, then add the track to the library before adding it to the
    // playlist.
    QFileInfo file(url.toLocalFile());

    // XXX: Possible WTF alert - Previously we thought we needed toString() here
    // but what you actually want in any case when converting a QUrl to a file
    // system path is QUrl::toLocalFile(). This is the second time we have
    // flip-flopped on this, but I think toLocalFile() should work in any
    // case. toString() absolutely does not work when you pass the result to a
    // QFileInfo. rryan 9/2010

    // Adds track, does not insert duplicates, handles unremoving logic.
    int trackId = m_trackDao.addTrack(file, true);

    // Do nothing if the location still isn't in the database.
    if (trackId < 0) {
        return false;
    }

    // appendTrackToPlaylist doesn't return whether it succeeded, so assume it
    // did.
    m_playlistDao.appendTrackToPlaylist(trackId, playlistId);
    return true;
}

bool PlaylistFeature::dragMoveAccept(QUrl url) {
    return false;
}

bool PlaylistFeature::dragMoveAcceptChild(const QModelIndex& index, QUrl url) {
    //TODO: Filter by supported formats regex and reject anything that doesn't match.

    QString playlistName = index.data().toString();
    int playlistId = m_playlistDao.getPlaylistIdFromName(playlistName);
    bool locked = m_playlistDao.isPlaylistLocked(playlistId);

    QFileInfo file(url.toLocalFile());
    bool formatSupported = SoundSourceProxy::isFilenameSupported(file.fileName());
    return !locked && formatSupported;
}

TreeItemModel* PlaylistFeature::getChildModel() {
    return &m_childModel;
}
/**
  * Purpose: When inserting or removing playlists,
  * we require the sidebar model not to reset.
  * This method queries the database and does dynamic insertion
*/
void PlaylistFeature::constructChildModel()
{
    QList<TreeItem*> data_list;
    int nameColumn = m_playlistTableModel.record().indexOf("name");
    int idColumn = m_playlistTableModel.record().indexOf("id");

    //Access the invisible root item
    TreeItem* root = m_childModel.getItem(QModelIndex());
    //Create new TreeItems for the playlists in the database
    for (int row = 0; row < m_playlistTableModel.rowCount(); ++row) {
        QModelIndex ind = m_playlistTableModel.index(row, nameColumn);
        QString playlist_name = m_playlistTableModel.data(ind).toString();
        ind = m_playlistTableModel.index(row, idColumn);
        int playlist_id = m_playlistTableModel.data(ind).toInt();
        bool locked = m_playlistDao.isPlaylistLocked(playlist_id);

        //Create the TreeItem whose parent is the invisible root item
        TreeItem* item = new TreeItem(playlist_name, playlist_name, this, root);
        item->setIcon(locked ? QIcon(":/images/library/ic_library_locked.png") : QIcon());
        data_list.append(item);
    }

    //Append all the newly created TreeItems in a dynamic way to the childmodel
    m_childModel.insertRows(data_list, 0, m_playlistTableModel.rowCount());
}

/**
  * Clears the child model dynamically, but the invisible root item remains
  */
void PlaylistFeature::clearChildModel()
{
    m_childModel.removeRows(0,m_playlistTableModel.rowCount());
}
void PlaylistFeature::slotImportPlaylist()
{
    qDebug() << "slotImportPlaylist() row:" ; //<< m_lastRightClickedIndex.data();

    QString playlist_file = QFileDialog::getOpenFileName
            (
            NULL,
            tr("Import Playlist"),
            QDesktopServices::storageLocation(QDesktopServices::MusicLocation),
            tr("Playlist Files (*.m3u *.pls)")
            );
    //Exit method if user cancelled the open dialog.
    if (playlist_file.isNull() || playlist_file.isEmpty() ) return;

    Parser* playlist_parser = NULL;

    if(playlist_file.endsWith(".m3u", Qt::CaseInsensitive))
    {
        playlist_parser = new ParserM3u();
    }
    else if(playlist_file.endsWith(".pls", Qt::CaseInsensitive))
    {
        playlist_parser = new ParserPls();
    }
    else
    {
        return;
    }
    QList<QString> entries = playlist_parser->parse(playlist_file);

    //Iterate over the List that holds URLs of playlist entires
    for (int i = 0; i < entries.size(); ++i) {
        m_pPlaylistTableModel->addTrack(QModelIndex(), entries[i]);

    }

    //delete the parser object
    if(playlist_parser) delete playlist_parser;
}
void PlaylistFeature::onLazyChildExpandation(const QModelIndex &index){
    //Nothing to do because the childmodel is not of lazy nature.
}
void PlaylistFeature::slotExportPlaylist(){
    qDebug() << "Export playlist" << m_lastRightClickedIndex.data();
    QString file_location = QFileDialog::getSaveFileName(NULL,
                                        tr("Export Playlist"),
                                        QDesktopServices::storageLocation(QDesktopServices::MusicLocation),
                                        tr("M3U Playlist (*.m3u);;PLS Playlist (*.pls)"));
    //Exit method if user cancelled the open dialog.
    if(file_location.isNull() || file_location.isEmpty()) return;
    //create and populate a list of files of the playlist
    QList<QString> playlist_items;
    int rows = m_pPlaylistTableModel->rowCount();
    for(int i = 0; i < rows; ++i){
        QModelIndex index = m_pPlaylistTableModel->index(i,0);
        playlist_items << m_pPlaylistTableModel->getTrackLocation(index);
    }
    //check config if relative paths are desired
    bool useRelativePath = (bool)m_pConfig->getValueString(ConfigKey("[Library]","UseRelativePathOnExport")).toInt();

    if(file_location.endsWith(".m3u", Qt::CaseInsensitive))
    {
        ParserM3u::writeM3UFile(file_location, playlist_items, useRelativePath);
    }
    else if(file_location.endsWith(".pls", Qt::CaseInsensitive))
    {
        ParserPls::writePLSFile(file_location,playlist_items, useRelativePath);
    }
    else
    {
        //default export to M3U if file extension is missing

        qDebug() << "Playlist export: No file extension specified. Appending .m3u "
                 << "and exporting to M3U.";
        file_location.append(".m3u");
        ParserM3u::writeM3UFile(file_location, playlist_items, useRelativePath);
    }

}
void PlaylistFeature::slotAddToAutoDJ() {
    //qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();

    if (m_lastRightClickedIndex.isValid()) {
        int playlistId = m_playlistDao.getPlaylistIdFromName(
            m_lastRightClickedIndex.data().toString());
        if (playlistId >= 0) {
            m_playlistDao.addToAutoDJQueue(playlistId);
        }
    }
    emit(featureUpdated());
}
