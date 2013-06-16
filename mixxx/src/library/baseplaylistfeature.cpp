#include "library/baseplaylistfeature.h"

#include "library/parser.h"
#include "library/parserm3u.h"
#include "library/parserpls.h"
#include "library/parsercsv.h"
#include "library/playlisttablemodel.h"
#include "library/trackcollection.h"
#include "mixxxkeyboard.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarysidebar.h"
#include "widget/wlibrarytextbrowser.h"

BasePlaylistFeature::BasePlaylistFeature(
    QObject* parent, ConfigObject<ConfigValue>* pConfig,
    TrackCollection* pTrackCollection,
    QString rootViewName)
        : LibraryFeature(parent),
          m_pConfig(pConfig),
          m_pTrackCollection(pTrackCollection),
          m_playlistDao(pTrackCollection->getPlaylistDAO()),
          m_trackDao(pTrackCollection->getTrackDAO()),
          m_pPlaylistTableModel(NULL),
          m_playlistTableModel(this, pTrackCollection->getDatabase()),
          m_rootViewName(rootViewName) {
    m_pCreatePlaylistAction = new QAction(tr("New Playlist"),this);
    connect(m_pCreatePlaylistAction, SIGNAL(triggered()),
            this, SLOT(slotCreatePlaylist()));

    m_pAddToAutoDJAction = new QAction(tr("Add to Auto DJ Queue (bottom)"), this);
    connect(m_pAddToAutoDJAction, SIGNAL(triggered()),
            this, SLOT(slotAddToAutoDJ()));

    m_pAddToAutoDJTopAction = new QAction(tr("Add to Auto DJ Queue (top)"), this);
    connect(m_pAddToAutoDJTopAction, SIGNAL(triggered()),
            this, SLOT(slotAddToAutoDJTop()));

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

    connect(&m_playlistDao, SIGNAL(added(int)),
            this, SLOT(slotPlaylistTableChanged(int)));

    connect(&m_playlistDao, SIGNAL(deleted(int)),
            this, SLOT(slotPlaylistTableChanged(int)));

    connect(&m_playlistDao, SIGNAL(renamed(int)),
            this, SLOT(slotPlaylistTableChanged(int)));

    connect(&m_playlistDao, SIGNAL(lockChanged(int)),
            this, SLOT(slotPlaylistTableChanged(int)));
}

BasePlaylistFeature::~BasePlaylistFeature() {
    delete m_pPlaylistTableModel;
    delete m_pCreatePlaylistAction;
    delete m_pDeletePlaylistAction;
    delete m_pImportPlaylistAction;
    delete m_pExportPlaylistAction;
    delete m_pAddToAutoDJAction;
    delete m_pAddToAutoDJTopAction;
    delete m_pRenamePlaylistAction;
    delete m_pLockPlaylistAction;
}

void BasePlaylistFeature::activate() {
    emit(switchToView(m_rootViewName));
    emit(restoreSearch(QString())); // Null String disables search box
}

void BasePlaylistFeature::activateChild(const QModelIndex& index) {
    //qDebug() << "BasePlaylistFeature::activateChild()" << index;

    // Switch the playlist table model's playlist.
    QString playlistName = index.data().toString();
    int playlistId = m_playlistDao.getPlaylistIdFromName(playlistName);
    if (m_pPlaylistTableModel) {
        m_pPlaylistTableModel->setPlaylist(playlistId);
        emit(showTrackModel(m_pPlaylistTableModel));
    }
}

void BasePlaylistFeature::slotRenamePlaylist() {
    QString oldName = m_lastRightClickedIndex.data().toString();
    int playlistId = m_playlistDao.getPlaylistIdFromName(oldName);
    bool locked = m_playlistDao.isPlaylistLocked(playlistId);

    if (locked) {
        qDebug() << "Skipping playlist rename because playlist" << playlistId
                 << "is locked.";
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
}

void BasePlaylistFeature::slotTogglePlaylistLock() {
    QString playlistName = m_lastRightClickedIndex.data().toString();
    int playlistId = m_playlistDao.getPlaylistIdFromName(playlistName);
    bool locked = !m_playlistDao.isPlaylistLocked(playlistId);

    if (!m_playlistDao.setPlaylistLocked(playlistId, locked)) {
        qDebug() << "Failed to toggle lock of playlistId " << playlistId;
    }
}

void BasePlaylistFeature::slotCreatePlaylist() {
    if (!m_pPlaylistTableModel) {
        return;
    }

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
        } else if (name.isEmpty()) {
            QMessageBox::warning(NULL,
                                 tr("Playlist Creation Failed"),
                                 tr("A playlist cannot have a blank name."));
        } else {
            validNameGiven = true;
        }

    } while (!validNameGiven);

    int playlistId = m_playlistDao.createPlaylist(name);

    if (playlistId != -1) {
        emit(showTrackModel(m_pPlaylistTableModel));
    }
    else {
        QMessageBox::warning(NULL,
                             tr("Playlist Creation Failed"),
                             tr("An unknown error occurred while creating playlist: ")
                              + name);
    }
}

void BasePlaylistFeature::slotDeletePlaylist() {
    //qDebug() << "slotDeletePlaylist() row:" << m_lastRightClickedIndex.data();
    int playlistId = m_playlistDao.getPlaylistIdFromName(m_lastRightClickedIndex.data().toString());
    bool locked = m_playlistDao.isPlaylistLocked(playlistId);

    if (locked) {
        qDebug() << "Skipping playlist deletion because playlist" << playlistId << "is locked.";
        return;
    }

    if (m_lastRightClickedIndex.isValid()) {
        Q_ASSERT(playlistId >= 0);

        m_playlistDao.deletePlaylist(playlistId);
        activate();
    }
}

bool BasePlaylistFeature::dropAccept(QUrl url) {
    Q_UNUSED(url);
    return false;
}

bool BasePlaylistFeature::dragMoveAccept(QUrl url) {
    Q_UNUSED(url);
    return false;
}

void BasePlaylistFeature::slotImportPlaylist() {
    qDebug() << "slotImportPlaylist() row:" ; //<< m_lastRightClickedIndex.data();

    if (!m_pPlaylistTableModel) {
        return;
    }

    QString playlist_file = QFileDialog::getOpenFileName(
        NULL,
        tr("Import Playlist"),
        QDesktopServices::storageLocation(QDesktopServices::MusicLocation),
        tr("Playlist Files (*.m3u *.m3u8 *.pls *.csv)"));
    // Exit method if user cancelled the open dialog.
    if (playlist_file.isNull() || playlist_file.isEmpty()) {
        return;
    }

    Parser* playlist_parser = NULL;

    if (playlist_file.endsWith(".m3u", Qt::CaseInsensitive) ||
        playlist_file.endsWith(".m3u8", Qt::CaseInsensitive)) {
        playlist_parser = new ParserM3u();
    } else if (playlist_file.endsWith(".pls", Qt::CaseInsensitive)) {
        playlist_parser = new ParserPls();
    } else if (playlist_file.endsWith(".csv", Qt::CaseInsensitive)) {
        playlist_parser = new ParserCsv();
    } else {
        return;
    }
    QList<QString> entries = playlist_parser->parse(playlist_file);

    // Iterate over the List that holds URLs of playlist entires
    for (int i = 0; i < entries.size(); ++i) {
        m_pPlaylistTableModel->addTrack(QModelIndex(), entries[i]);
    }

    // delete the parser object
    if (playlist_parser) {
        delete playlist_parser;
    }
}

void BasePlaylistFeature::slotExportPlaylist() {
    if (!m_pPlaylistTableModel) {
        return;
    }

    qDebug() << "Export playlist" << m_lastRightClickedIndex.data();
    // Open a dialog to let the user choose the file location for playlist export.
    // By default, the directory is set to the OS's Music directory and the file
    // name to the playlist name.
    QString playlist_filename = m_lastRightClickedIndex.data().toString();
    QString music_directory = QDesktopServices::storageLocation(QDesktopServices::MusicLocation);
    QString file_location = QFileDialog::getSaveFileName(
        NULL,
        tr("Export Playlist"),
        music_directory.append("/").append(playlist_filename),
        tr("M3U Playlist (*.m3u);;M3U8 Playlist (*.m3u8);;"
           "PLS Playlist (*.pls);;Text CSV (*.csv);;Readable Text (*.txt)"));
    // Exit method if user cancelled the open dialog.
    if (file_location.isNull() || file_location.isEmpty()) {
        return;
    }

    // Create a new table model since the main one might have an active search.
    QScopedPointer<PlaylistTableModel> pPlaylistTableModel(
        new PlaylistTableModel(this, m_pTrackCollection,
                               "mixxx.db.model.playlist_export"));

    pPlaylistTableModel->setPlaylist(m_pPlaylistTableModel->getPlaylist());
    pPlaylistTableModel->setSort(pPlaylistTableModel->fieldIndex(PLAYLISTTRACKSTABLE_POSITION), Qt::AscendingOrder);
    pPlaylistTableModel->select();

    // check config if relative paths are desired
    bool useRelativePath = static_cast<bool>(m_pConfig->getValueString(
        ConfigKey("[Library]", "UseRelativePathOnExport")).toInt());

    if (file_location.endsWith(".csv", Qt::CaseInsensitive)) {
        ParserCsv::writeCSVFile(file_location, pPlaylistTableModel.data(), useRelativePath);
    } else if (file_location.endsWith(".txt", Qt::CaseInsensitive)) {
        if (m_playlistDao.getHiddenType(pPlaylistTableModel->getPlaylist()) == PlaylistDAO::PLHT_SET_LOG) {
            ParserCsv::writeReadableTextFile(file_location, pPlaylistTableModel.data(), true);
        } else {
            ParserCsv::writeReadableTextFile(file_location, pPlaylistTableModel.data(), false);
        }
    } else {
        // Create and populate a list of files of the playlist
        QList<QString> playlist_items;
        int rows = pPlaylistTableModel->rowCount();
        for (int i = 0; i < rows; ++i) {
            QModelIndex index = pPlaylistTableModel->index(i, 0);
            playlist_items << pPlaylistTableModel->getTrackLocation(index);
        }

        if (file_location.endsWith(".pls", Qt::CaseInsensitive)) {
            ParserPls::writePLSFile(file_location, playlist_items, useRelativePath);
        } else if (file_location.endsWith(".m3u8", Qt::CaseInsensitive)) {
            ParserM3u::writeM3U8File(file_location, playlist_items, useRelativePath);
        } else {
            //default export to M3U if file extension is missing
            if(!file_location.endsWith(".m3u", Qt::CaseInsensitive))
            {
                qDebug() << "Crate export: No valid file extension specified. Appending .m3u "
                         << "and exporting to M3U.";
                file_location.append(".m3u");
            }
            ParserM3u::writeM3UFile(file_location, playlist_items, useRelativePath);
        }
    }
}

void BasePlaylistFeature::slotAddToAutoDJ() {
    //qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();
    addToAutoDJ(false); // Top = True
}

void BasePlaylistFeature::slotAddToAutoDJTop() {
    //qDebug() << "slotAddToAutoDJTop() row:" << m_lastRightClickedIndex.data();
    addToAutoDJ(true); // bTop = True
}

void BasePlaylistFeature::addToAutoDJ(bool bTop) {
    //qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();

    if (m_lastRightClickedIndex.isValid()) {
        int playlistId = m_playlistDao.getPlaylistIdFromName(
            m_lastRightClickedIndex.data().toString());
        if (playlistId >= 0) {
            // Insert this playlist
            m_playlistDao.addToAutoDJQueue(playlistId, bTop);
        }
    }
}

void BasePlaylistFeature::onLazyChildExpandation(const QModelIndex &index){
    Q_UNUSED(index);
    //Nothing to do because the childmodel is not of lazy nature.
}

TreeItemModel* BasePlaylistFeature::getChildModel() {
    return &m_childModel;
}

void BasePlaylistFeature::bindWidget(WLibrarySidebar* sidebarWidget,
                                 WLibrary* libraryWidget,
                                 MixxxKeyboard* keyboard) {
    Q_UNUSED(sidebarWidget);
    Q_UNUSED(keyboard);
    WLibraryTextBrowser* edit = new WLibraryTextBrowser(libraryWidget);
    edit->setHtml(getRootViewHtml());
    libraryWidget->registerView(m_rootViewName, edit);
}

/**
  * Clears the child model dynamically, but the invisible root item remains
  */
void BasePlaylistFeature::clearChildModel() {
    m_childModel.removeRows(0,m_playlistTableModel.rowCount());
}

