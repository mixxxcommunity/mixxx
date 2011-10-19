// cratefeature.cpp
// Created 10/22/2009 by RJ Ryan (rryan@mit.edu)

#include <QInputDialog>
#include <QMenu>
#include <QLineEdit>

#include "library/cratefeature.h"
#include "library/parser.h"
#include "library/parserm3u.h"
#include "library/parserpls.h"

#include "library/cratetablemodel.h"
#include "library/trackcollection.h"
#include "widget/wlibrarytextbrowser.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarysidebar.h"
#include "mixxxkeyboard.h"
#include "treeitem.h"
#include "soundsourceproxy.h"

CrateFeature::CrateFeature(QObject* parent,
                           TrackCollection* pTrackCollection, ConfigObject<ConfigValue>* pConfig)
        : m_pTrackCollection(pTrackCollection),
          m_crateListTableModel(this, pTrackCollection->getDatabase()),
          m_pConfig(pConfig),
          m_crateTableModel(this, pTrackCollection) {
    m_pCreateCrateAction = new QAction(tr("New Crate"),this);
    connect(m_pCreateCrateAction, SIGNAL(triggered()),
            this, SLOT(slotCreateCrate()));

    m_pDeleteCrateAction = new QAction(tr("Remove"),this);
    connect(m_pDeleteCrateAction, SIGNAL(triggered()),
            this, SLOT(slotDeleteCrate()));

    m_pRenameCrateAction = new QAction(tr("Rename"),this);
    connect(m_pRenameCrateAction, SIGNAL(triggered()),
            this, SLOT(slotRenameCrate()));

    m_pLockCrateAction = new QAction(tr("Lock"),this);
    connect(m_pLockCrateAction, SIGNAL(triggered()),
            this, SLOT(slotToggleCrateLock()));

    m_pImportPlaylistAction = new QAction(tr("Import Crate"),this);
    connect(m_pImportPlaylistAction, SIGNAL(triggered()),
            this, SLOT(slotImportPlaylist()));
    m_pExportPlaylistAction = new QAction(tr("Export Crate"), this);
    connect(m_pExportPlaylistAction, SIGNAL(triggered()),
            this, SLOT(slotExportPlaylist()));

    m_crateListTableModel.setTable("crates");
    m_crateListTableModel.setSort(m_crateListTableModel.fieldIndex("name"),
                                  Qt::AscendingOrder);
    m_crateListTableModel.setFilter("show = 1");
    m_crateListTableModel.select();

    // construct child model
    TreeItem *rootItem = new TreeItem();
    m_childModel.setRootItem(rootItem);
    constructChildModel();
}

CrateFeature::~CrateFeature() {
    //delete QActions
    delete m_pCreateCrateAction;
    delete m_pDeleteCrateAction;
    delete m_pRenameCrateAction;
    delete m_pLockCrateAction;
    delete m_pImportPlaylistAction;
}

QVariant CrateFeature::title() {
    return tr("Crates");
}

QIcon CrateFeature::getIcon() {
    return QIcon(":/images/library/ic_library_crates.png");
}

bool CrateFeature::dropAccept(QUrl url) {
    return false;
}

bool CrateFeature::dropAcceptChild(const QModelIndex& index, QUrl url) {
    QString crateName = index.data().toString();
    int crateId = m_pTrackCollection->getCrateDAO().getCrateIdByName(crateName);

    //XXX: See the comment in PlaylistFeature::dropAcceptChild() about
    //     QUrl::toLocalFile() vs. QUrl::toString() usage.
    QFileInfo file(url.toLocalFile());

    // Adds track, does not insert duplicates, handles unremoving logic.
    int trackId = m_pTrackCollection->getTrackDAO().addTrack(file, true);

    qDebug() << "CrateFeature::dropAcceptChild adding track"
             << trackId << "to crate" << crateId;

    CrateDAO& crateDao = m_pTrackCollection->getCrateDAO();

    if (trackId >= 0)
        return crateDao.addTrackToCrate(trackId, crateId);
    return false;
}

bool CrateFeature::dragMoveAccept(QUrl url) {
    return false;
}

bool CrateFeature::dragMoveAcceptChild(const QModelIndex& index, QUrl url) {
    //TODO: Filter by supported formats regex and reject anything that doesn't match.
    QString crateName = index.data().toString();
    CrateDAO& crateDao = m_pTrackCollection->getCrateDAO();
    int crateId = crateDao.getCrateIdByName(crateName);
    bool locked = crateDao.isCrateLocked(crateId);

    QFileInfo file(url.toLocalFile());
    bool formatSupported = SoundSourceProxy::isFilenameSupported(file.fileName());
    return !locked && formatSupported;
}

void CrateFeature::bindWidget(WLibrarySidebar* sidebarWidget,
                              WLibrary* libraryWidget,
                              MixxxKeyboard* keyboard) {
    WLibraryTextBrowser* edit = new WLibraryTextBrowser(libraryWidget);
    connect(this, SIGNAL(showPage(const QUrl&)),
            edit, SLOT(setSource(const QUrl&)));
    libraryWidget->registerView("CRATEHOME", edit);
}

TreeItemModel* CrateFeature::getChildModel() {
    return &m_childModel;
}

void CrateFeature::activate() {
    emit(showPage(QUrl("qrc:/html/crates.html")));
    emit(switchToView("CRATEHOME"));
}

void CrateFeature::activateChild(const QModelIndex& index) {
    if (!index.isValid())
        return;
    QString crateName = index.data().toString();
    int crateId = m_pTrackCollection->getCrateDAO().getCrateIdByName(crateName);
    m_crateTableModel.setCrate(crateId);
    emit(showTrackModel(&m_crateTableModel));
}

void CrateFeature::onRightClick(const QPoint& globalPos) {
    m_lastRightClickedIndex = QModelIndex();
    QMenu menu(NULL);
    menu.addAction(m_pCreateCrateAction);
    menu.exec(globalPos);
}

void CrateFeature::onRightClickChild(const QPoint& globalPos, QModelIndex index) {
    //Save the model index so we can get it in the action slots...
    m_lastRightClickedIndex = index;

    QString crateName = index.data().toString();
    CrateDAO& crateDAO = m_pTrackCollection->getCrateDAO();
    int crateId = crateDAO.getCrateIdByName(crateName);

    bool locked = crateDAO.isCrateLocked(crateId);

    m_pDeleteCrateAction->setEnabled(!locked);
    m_pRenameCrateAction->setEnabled(!locked);

    m_pLockCrateAction->setText(locked ? tr("Unlock") : tr("Lock"));

    QMenu menu(NULL);
    menu.addAction(m_pCreateCrateAction);
    menu.addSeparator();
    menu.addAction(m_pRenameCrateAction);
    menu.addAction(m_pDeleteCrateAction);
    menu.addAction(m_pLockCrateAction);
    menu.addSeparator();
    menu.addAction(m_pImportPlaylistAction);
    menu.addAction(m_pExportPlaylistAction);
    menu.exec(globalPos);
}

void CrateFeature::slotCreateCrate() {

    QString name;
    bool validNameGiven = false;
    CrateDAO& crateDao = m_pTrackCollection->getCrateDAO();

    do {
        bool ok = false;
        name = QInputDialog::getText(NULL,
                                     tr("New Crate"),
                                     tr("Crate name:"),
                                     QLineEdit::Normal, tr("New Crate"),
                                     &ok).trimmed();

        if (!ok)
            return;

        int existingId = crateDao.getCrateIdByName(name);

        if (existingId != -1) {
            QMessageBox::warning(NULL,
                                 tr("Creating Crate Failed"),
                                 tr("A crate by that name already exists."));
        }
        else if (name.isEmpty()) {
            QMessageBox::warning(NULL,
                                 tr("Creating Crate Failed"),
                                 tr("A crate cannot have a blank name."));
        }
        else {
            validNameGiven = true;
        }

    } while (!validNameGiven);

    bool crateCreated = crateDao.createCrate(name);

    if (crateCreated) {
        clearChildModel();
        m_crateListTableModel.select();
        constructChildModel();
        // Switch to the new crate.
        int crate_id = crateDao.getCrateIdByName(name);
        m_crateTableModel.setCrate(crate_id);
        emit(showTrackModel(&m_crateTableModel));
        // TODO(XXX) set sidebar selection
        emit(featureUpdated());
    } else {
        qDebug() << "Error creating crate with name " << name;
        QMessageBox::warning(NULL,
                             tr("Creating Crate Failed"),
                             tr("An unknown error occurred while creating crate: ")
                             + name);

    }
}

void CrateFeature::slotDeleteCrate() {
    QString crateName = m_lastRightClickedIndex.data().toString();
    CrateDAO &crateDao = m_pTrackCollection->getCrateDAO();
    int crateId = crateDao.getCrateIdByName(crateName);
    bool locked = crateDao.isCrateLocked(crateId);

    if (locked) {
        qDebug() << "Skipping crate deletion because crate" << crateId << "is locked.";
        return;
    }

    bool deleted = crateDao.deleteCrate(crateId);

    if (deleted) {
        clearChildModel();
        m_crateListTableModel.select();
        constructChildModel();
        emit(featureUpdated());
    } else {
        qDebug() << "Failed to delete crateId" << crateId;
    }
}

void CrateFeature::slotRenameCrate() {
    QString oldName = m_lastRightClickedIndex.data().toString();
    CrateDAO &crateDao = m_pTrackCollection->getCrateDAO();
    int crateId = crateDao.getCrateIdByName(oldName);
    bool locked = crateDao.isCrateLocked(crateId);

    if (locked) {
        qDebug() << "Skipping crate rename because crate" << crateId << "is locked.";
        return;
    }

    QString newName;
    bool validNameGiven = false;

    do {
        bool ok = false;
        newName = QInputDialog::getText(NULL,
                                        tr("Rename Crate"),
                                        tr("New crate name:"),
                                        QLineEdit::Normal,
                                        oldName,
                                        &ok).trimmed();

        if (!ok || newName == oldName) {
            return;
        }

        int existingId = m_pTrackCollection->getCrateDAO().getCrateIdByName(newName);

        if (existingId != -1) {
            QMessageBox::warning(NULL,
                                tr("Renaming Crate Failed"),
                                tr("A crate by that name already exists."));
        }
        else if (newName.isEmpty()) {
            QMessageBox::warning(NULL,
                                tr("Renaming Crate Failed"),
                                tr("A crate cannot have a blank name."));
        }
        else {
            validNameGiven = true;
        }
    } while (!validNameGiven);


    if (m_pTrackCollection->getCrateDAO().renameCrate(crateId, newName)) {
        clearChildModel();
        m_crateListTableModel.select();
        constructChildModel();
        emit(featureUpdated());
        m_crateTableModel.setCrate(crateId);
    } else {
        qDebug() << "Failed to rename crateId" << crateId;
    }
}

void CrateFeature::slotToggleCrateLock()
{
    QString crateName = m_lastRightClickedIndex.data().toString();
    CrateDAO& crateDAO = m_pTrackCollection->getCrateDAO();
    int crateId = crateDAO.getCrateIdByName(crateName);
    bool locked = !crateDAO.isCrateLocked(crateId);

    if (!crateDAO.setCrateLocked(crateId, locked)) {
        qDebug() << "Failed to toggle lock of crateId " << crateId;
    }

    TreeItem* crateItem = m_childModel.getItem(m_lastRightClickedIndex);
    crateItem->setIcon(
        locked ? QIcon(":/images/library/ic_library_locked.png") : QIcon());
}


/**
  * Purpose: When inserting or removing playlists,
  * we require the sidebar model not to reset.
  * This method queries the database and does dynamic insertion
*/
void CrateFeature::constructChildModel()
{
    QList<TreeItem*> data_list;
    int nameColumn = m_crateListTableModel.record().indexOf("name");
    int idColumn = m_crateListTableModel.record().indexOf("id");
    //Access the invisible root item
    TreeItem* root = m_childModel.getItem(QModelIndex());
    CrateDAO &crateDao = m_pTrackCollection->getCrateDAO();

    for (int row = 0; row < m_crateListTableModel.rowCount(); ++row) {
            QModelIndex ind = m_crateListTableModel.index(row, nameColumn);
            QString crate_name = m_crateListTableModel.data(ind).toString();
            ind = m_crateListTableModel.index(row, idColumn);
            int crate_id = m_crateListTableModel.data(ind).toInt();

            //Create the TreeItem whose parent is the invisible root item
            TreeItem* item = new TreeItem(crate_name, crate_name, this, root);
            bool locked = crateDao.isCrateLocked(crate_id);
            item->setIcon(locked ? QIcon(":/images/library/ic_library_locked.png") : QIcon());
            data_list.append(item);
    }
    //Append all the newly created TreeItems in a dynamic way to the childmodel
    m_childModel.insertRows(data_list, 0, m_crateListTableModel.rowCount());
}

/**
  * Clears the child model dynamically
  */
void CrateFeature::clearChildModel()
{
    m_childModel.removeRows(0,m_crateListTableModel.rowCount());
}

void CrateFeature::slotImportPlaylist()
{
    qDebug() << "slotImportPlaylist() row:" ; //<< m_lastRightClickedIndex.data();


    QString playlist_file = QFileDialog::getOpenFileName(
        NULL,
        tr("Import Playlist"),
        QDesktopServices::storageLocation(QDesktopServices::MusicLocation),
        tr("Playlist Files (*.m3u *.pls)"));
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
    //qDebug() << "Size of Imported Playlist: " << entries.size();

    //Iterate over the List that holds URLs of playlist entires
    for (int i = 0; i < entries.size(); ++i) {
        m_crateTableModel.addTrack(QModelIndex(), entries[i]);
        //qDebug() << "Playlist entry: " << entries[i];

    }

    //delete the parser object
    if(playlist_parser)
        delete playlist_parser;
}
void CrateFeature::onLazyChildExpandation(const QModelIndex &index){
    //Nothing to do because the childmodel is not of lazy nature.
}
void CrateFeature::slotExportPlaylist(){
    qDebug() << "Export playlist" << m_lastRightClickedIndex.data();
    QString file_location = QFileDialog::getSaveFileName(NULL,
                                        tr("Export Playlist"),
                                        QDesktopServices::storageLocation(QDesktopServices::MusicLocation),
                                        tr("M3U Playlist (*.m3u);;PLS Playlist (*.pls)"));
    //Exit method if user cancelled the open dialog.
    if(file_location.isNull() || file_location.isEmpty()) return;
    //create and populate a list of files of the playlist
    QList<QString> playlist_items;
    int rows = m_crateTableModel.rowCount();
    for (int i = 0; i < rows; ++i) {
        QModelIndex index = m_crateTableModel.index(i, 0);
        playlist_items << m_crateTableModel.getTrackLocation(index);
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

        qDebug() << "Crate export: No file extension specified. Appending .m3u "
                 << "and exporting to M3U.";
        file_location.append(".m3u");
        ParserM3u::writeM3UFile(file_location, playlist_items, useRelativePath);
    }
}
