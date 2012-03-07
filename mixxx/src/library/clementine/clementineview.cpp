// clementineview.cpp
// Created 2/25/2011 by Daniel Schürmann (daschuer@gmx.de)
//
// This shows the Clementine-Player library in Mixxx



#include "clementineview.h"
#include "clementinefeature.h"
#include "trackinfoobject.h"
#include "track/beatfactory.h"

// from clementine
#include "library/libraryviewcontainer.h"
#include "library/libraryview.h"
#include "library/library.h"
#include "library/librarymodel.h"
#include "library/libraryfilterwidget.h"
#include "core/taskmanager.h"
#include "playlist/songmimedata.h"

#include <QSortFilterProxyModel>
#include <QModelIndexList>


ClementineView::ClementineView(QWidget* parent, TrackCollection* pTrackCollection)
    : QWidget(parent),
      m_libraryViewContainer(new LibraryViewContainer(this)),
      m_librarySortModel(new QSortFilterProxyModel(this)),
      m_pTrackCollection(pTrackCollection) {



}

ClementineView::~ClementineView() {
}

void ClementineView::connectLibrary(Library* library, TaskManager* task_manager) {

    qDebug() << "Creating models";
    m_librarySortModel->setSourceModel((QAbstractItemModel*)library->model());
    m_librarySortModel->setSortRole(LibraryModel::Role_SortText);
    m_librarySortModel->setDynamicSortFilter(true);
    m_librarySortModel->sort(0);


    m_libraryViewContainer->view()->setModel((QAbstractItemModel *)m_librarySortModel);
    m_libraryViewContainer->view()->SetLibrary(library->model());
    m_libraryViewContainer->view()->SetTaskManager(task_manager);
    //m_libraryViewContainer->view()->SetDeviceManager(devices_);
    //m_libraryViewContainer->view()->SetCoverProviders(cover_providers_);

    connect(m_libraryViewContainer->view(), SIGNAL(RightClicked(QMimeData*)),
            this, SLOT(slotLibraryViewRightClicked(QMimeData*)));

    m_libraryViewContainer->view()->AddSongActionSeparator();

    // Auto DJ Actions
    m_libraryViewContainer->view()->AddSongAction(QIcon(),
            tr("Add to Auto DJ bottom"), this, SLOT(slotSendToAutoDJ()));
    m_libraryViewContainer->view()->AddSongAction(QIcon(),
            tr("Add to Auto DJ top 2"), this, SLOT(slotSendToAutoDJTop()));

    m_libraryViewContainer->view()->AddSongActionSeparator();


/*
    ControlObjectThreadMain* pNumSamplers = new ControlObjectThreadMain(
        ControlObject::getControl(ConfigKey("[Master]", "num_samplers")));
    ControlObjectThreadMain* pNumDecks = new ControlObjectThreadMain(
        ControlObject::getControl(ConfigKey("[Master]", "num_decks")));

    // Load to Deck Actions
    int iNumDecks = pNumDecks->get();
    for (int i = 1; i <= iNumDecks; ++i) {
        //QString deckGroup = QString("[Channel%1]").arg(i);
        //bool deckPlaying = ControlObject::getControl(
        //ConfigKey(deckGroup, "play"))->get() == 1.0f;
        //bool deckEnabled = !deckPlaying && oneSongSelected;
        m_libraryViewContainer->view()->AddSongAction(QIcon(),
                tr("Load to Deck %1").arg(i), &m_deckMapper, SLOT(map()));
        }
    }

    int iNumSamplers = m_pNumSamplers->get();
    if (iNumSamplers > 0) {
        QMenu menu = m_libraryViewContainer->view()->AddSongActionMenu();
        for (int i = 1; i <= iNumSamplers; ++i) {
            //QString samplerGroup = QString("[Sampler%1]").arg(i);
            //bool samplerPlaying = ControlObject::getControl(
            //        ConfigKey(samplerGroup, "play"))->get() == 1.0f;
            //bool samplerEnabled = !samplerPlaying && oneSongSelected;
            menu->addAction(QIcon();
                    tr("Sampler %1").arg(i), &m_samplerMapper, SLOT(map()));
        }
    }

    m_libraryViewContainer->view()->AddSongActionSeparator();


    PlaylistDAO& playlistDao = m_pTrackCollection->getPlaylistDAO();
    int numPlaylists = playlistDao.playlistCount();

    for (int i = 0; i < numPlaylists; ++i) {
        int iPlaylistId = playlistDao.getPlaylistId(i);

        if (!playlistDao.isHidden(iPlaylistId)) {

            QString playlistName = playlistDao.getPlaylistName(iPlaylistId);

            m_libraryViewContainer->view()->AddSongAction(QIcon(),
                         tr("Load to Deck %1").arg(i), &m_deckMapper, SLOT(map()));


            QAction* pAction = new QAction(playlistName, m_pPlaylistMenu);
            bool locked = playlistDao.isPlaylistLocked(iPlaylistId);
            pAction->setEnabled(!locked);
            m_pPlaylistMenu->addAction(pAction);
            m_playlistMapper.setMapping(pAction, iPlaylistId);
            connect(pAction, SIGNAL(triggered()), &m_playlistMapper, SLOT(map()));
        }
    }

    if (modelHasCapabilities(TrackModel::TRACKMODELCAPS_ADDTOCRATE)) {
        m_pCrateMenu->clear();
        CrateDAO& crateDao = m_pTrackCollection->getCrateDAO();
        int numCrates = crateDao.crateCount();
        for (int i = 0; i < numCrates; ++i) {
            int iCrateId = crateDao.getCrateId(i);
            // No leak because making the menu the parent means they will be
            // auto-deleted
            QAction* pAction = new QAction(crateDao.crateName(iCrateId), m_pCrateMenu);
            bool locked = crateDao.isCrateLocked(iCrateId);
            pAction->setEnabled(!locked);
            m_pCrateMenu->addAction(pAction);
            m_crateMapper.setMapping(pAction, iCrateId);
            connect(pAction, SIGNAL(triggered()), &m_crateMapper, SLOT(map()));
        }

        m_pMenu->addMenu(m_pCrateMenu);
    }

    bool locked = modelHasCapabilities(TrackModel::TRACKMODELCAPS_LOCKED);
    m_pRemoveAct->setEnabled(!locked);
    m_pMenu->addSeparator();
    if (modelHasCapabilities(TrackModel::TRACKMODELCAPS_REMOVE)) {
        m_pMenu->addAction(m_pRemoveAct);
    }
    if (modelHasCapabilities(TrackModel::TRACKMODELCAPS_RELOCATE)) {
        m_pMenu->addAction(m_pRelocate);
    }
    if (modelHasCapabilities(TrackModel::TRACKMODELCAPS_RELOADMETADATA)) {
        m_pMenu->addAction(m_pReloadMetadataAct);
    }
    m_pPropertiesAct->setEnabled(oneSongSelected);
    m_pMenu->addAction(m_pPropertiesAct);

    //Create the right-click menu
    m_pMenu->popup(event->globalPos());


    m_libraryViewContainer->filter()->SetLibraryModel(library->model());
    //m_libraryViewContainer->filter()->SetSettingsGroup(kSettingsGroup);
     *
     *
     */
}


void ClementineView::setupLibraryFilerWidget() {

    // Library filter widget
    //QActionGroup* library_view_group = new QActionGroup(this);

    //library_show_all_ = library_view_group->addAction(tr("Show all songs"));
    //library_show_duplicates_ = library_view_group->addAction(tr("Show only duplicates"));
    //library_show_untagged_ = library_view_group->addAction(tr("Show only untagged"));

    //library_show_all_->setCheckable(true);
    //library_show_duplicates_->setCheckable(true);
    //library_show_untagged_->setCheckable(true);
    //library_show_all_->setChecked(true);

    //connect(library_view_group, SIGNAL(triggered(QAction*)), SLOT(ChangeLibraryQueryMode(QAction*)));

    //QAction* library_config_action = new QAction(
    //    IconLoader::Load("configure"), tr("Configure library..."), this);
    //connect(library_config_action, SIGNAL(triggered()), SLOT(ShowLibraryConfig()));
    //m_libraryViewContainer->filter()->SetSettingsGroup(kSettingsGroup);
    //m_libraryViewContainer->filter()->SetLibraryModel(library_->model());

    //QAction* separator = new QAction(this);
    //separator->setSeparator(true);

    //m_libraryViewContainer->filter()->AddMenuAction(library_show_all_);
    //m_libraryViewContainer->filter()->AddMenuAction(library_show_duplicates_);
    //m_libraryViewContainer->filter()->AddMenuAction(library_show_untagged_);
    //m_libraryViewContainer->filter()->AddMenuAction(separator);
    //m_libraryViewContainer->filter()->AddMenuAction(library_config_action);

}

void ClementineView::slotSendToAutoDJ() {
    // append to auto DJ
    addToAutoDJ(false); // bTop = false
}

void ClementineView::slotSendToAutoDJTop() {
    addToAutoDJ(true); // bTop = true
}

void ClementineView::addToAutoDJ(bool bTop) {

    PlaylistDAO& playlistDao = m_pTrackCollection->getPlaylistDAO();
    int iAutoDJPlaylistId = playlistDao.getPlaylistIdFromName(AUTODJ_TABLE);

    if (iAutoDJPlaylistId == -1) {
        return;
    }

    if (const SongMimeData* songData = qobject_cast<const SongMimeData*>(m_pData)) {
        foreach (Song song, songData->songs) {
            qDebug() << "-----" << song.title();
            TrackPointer pTrack = getTrack(song);
            int iTrackId = pTrack->getId();
            if (iTrackId != -1) {
                if (bTop) {
                    // Load track to position two because position one is already loaded to the player
                    playlistDao.insertTrackIntoPlaylist(iTrackId, iAutoDJPlaylistId, 2);
                }
                else {
                    playlistDao.appendTrackToPlaylist(iTrackId, iAutoDJPlaylistId);
                }
            }
        }
    } else if (songData->hasUrls()) {
        foreach (QUrl url, m_pData->urls()) {
            qDebug() << "-----" << url;
            TrackPointer pTrack = getTrack(url);
            int iTrackId = pTrack->getId();
            if (iTrackId != -1) {
                if (bTop) {
                    // Load track to position two because position one is already loaded to the player
                    playlistDao.insertTrackIntoPlaylist(iTrackId, iAutoDJPlaylistId, 2);
                }
                else {
                    playlistDao.appendTrackToPlaylist(iTrackId, iAutoDJPlaylistId);
                }
            }
        }
    }
}

/*
void ClementineView::slotImportAsMixxxPlaylist() {
    // qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();

    if (m_lastRightClickedIndex.isValid()) {
        TreeItem *item = static_cast<TreeItem*>(m_lastRightClickedIndex.internalPointer());
        QString playlistName = item->data().toString();
        QString playlistStId = item->dataPath().toString();
        int playlistID = playlistStId.toInt();
        qDebug() << "BansheeFeature::slotImportAsMixxxPlaylist " << playlistName << " " << playlistStId;
        if (playlistID > 0) {
            BansheePlaylistModel* pPlaylistModelToAdd = new BansheePlaylistModel(this, m_pTrackCollection, &m_connection);
            pPlaylistModelToAdd->setPlaylist(playlistID);
            PlaylistDAO &playlistDao = m_pTrackCollection->getPlaylistDAO();

            int playlistId = playlistDao.getPlaylistIdFromName(playlistName);
            int i = 1;

            if (playlistId != -1) {
                // Calculate a unique name
                playlistName += "(%1)";
                while (playlistId != -1) {
                    i++;
                    playlistId = playlistDao.getPlaylistIdFromName(playlistName.arg(i));
                }
                playlistName = playlistName.arg(i);
            }
            playlistId = playlistDao.createPlaylist(playlistName);

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
                                      + playlistName);
            }

            delete pPlaylistModelToAdd;
            */
/*
        }
    }
    emit(featureUpdated());
}
*/

void ClementineView::loadSelectionToGroup(QString group) {
    // If the track load override is disabled, check to see if a track is
    // playing before trying to load it
 /*
    if ( !(m_pConfig->getValueString(ConfigKey("[Controls]","AllowTrackLoadToPlayingDeck")).toInt()) ) {
        bool groupPlaying = (ControlObject::getControl(
                ConfigKey(group, "play"))->get() == 1.0f);

        if (groupPlaying) {
            return;
        }
    }
 */

    TrackPointer pTrack;

    if (const SongMimeData* songData = qobject_cast<const SongMimeData*>(m_pData)) {
        foreach (Song song, songData->songs) {
            qDebug() << "-----" << song.title();
            pTrack = getTrack(song);
            break;
        }
    } else if (songData->hasUrls()) {
        foreach (QUrl url, m_pData->urls()) {
            qDebug() << "-----" << url;
            pTrack = getTrack(url);
            break;
        }
    }

    if (pTrack) {
//        emit(loadTrackToPlayer(pTrack, group));
    }
}

void ClementineView::slotLibraryViewRightClicked(QMimeData* data) {
    // setup Context Menu actions
    qDebug() << "slotLibraryViewRightClicked";

    m_pData = data;

    /*
        // Load to Deck Actions
        int iNumDecks = m_pNumDecks->get();
        for (int i = 1; i <= iNumDecks; ++i) {
            //QString deckGroup = QString("[Channel%1]").arg(i);
            //bool deckPlaying = ControlObject::getControl(
            //ConfigKey(deckGroup, "play"))->get() == 1.0f;
            //bool deckEnabled = !deckPlaying && oneSongSelected;
            m_libraryViewContainer->view()->AddSongAction(QIcon(),
                    tr("Load to Deck %1").arg(i), &m_deckMapper, SLOT(map()));
            }
        }

        int iNumSamplers = m_pNumSamplers->get();
        if (iNumSamplers > 0) {
            QMenu menu = m_libraryViewContainer->view()->AddSongActionMenu();
            for (int i = 1; i <= iNumSamplers; ++i) {
                //QString samplerGroup = QString("[Sampler%1]").arg(i);
                //bool samplerPlaying = ControlObject::getControl(
                //        ConfigKey(samplerGroup, "play"))->get() == 1.0f;
                //bool samplerEnabled = !samplerPlaying && oneSongSelected;
                menu->addAction(QIcon();
                        tr("Sampler %1").arg(i), &m_samplerMapper, SLOT(map()));
            }
        }

        m_libraryViewContainer->view()->AddSongActionSeparator();


        PlaylistDAO& playlistDao = m_pTrackCollection->getPlaylistDAO();
        int numPlaylists = playlistDao.playlistCount();

        for (int i = 0; i < numPlaylists; ++i) {
            int iPlaylistId = playlistDao.getPlaylistId(i);

            if (!playlistDao.isHidden(iPlaylistId)) {

                QString playlistName = playlistDao.getPlaylistName(iPlaylistId);

                m_libraryViewContainer->view()->AddSongAction(QIcon(),
                             tr("Load to Deck %1").arg(i), &m_deckMapper, SLOT(map()));


                QAction* pAction = new QAction(playlistName, m_pPlaylistMenu);
                bool locked = playlistDao.isPlaylistLocked(iPlaylistId);
                pAction->setEnabled(!locked);
                m_pPlaylistMenu->addAction(pAction);
                m_playlistMapper.setMapping(pAction, iPlaylistId);
                connect(pAction, SIGNAL(triggered()), &m_playlistMapper, SLOT(map()));
            }
        }

        if (modelHasCapabilities(TrackModel::TRACKMODELCAPS_ADDTOCRATE)) {
            m_pCrateMenu->clear();
            CrateDAO& crateDao = m_pTrackCollection->getCrateDAO();
            int numCrates = crateDao.crateCount();
            for (int i = 0; i < numCrates; ++i) {
                int iCrateId = crateDao.getCrateId(i);
                // No leak because making the menu the parent means they will be
                // auto-deleted
                QAction* pAction = new QAction(crateDao.crateName(iCrateId), m_pCrateMenu);
                bool locked = crateDao.isCrateLocked(iCrateId);
                pAction->setEnabled(!locked);
                m_pCrateMenu->addAction(pAction);
                m_crateMapper.setMapping(pAction, iCrateId);
                connect(pAction, SIGNAL(triggered()), &m_crateMapper, SLOT(map()));
            }

            m_pMenu->addMenu(m_pCrateMenu);
        }

        bool locked = modelHasCapabilities(TrackModel::TRACKMODELCAPS_LOCKED);
        m_pRemoveAct->setEnabled(!locked);
        m_pMenu->addSeparator();
        if (modelHasCapabilities(TrackModel::TRACKMODELCAPS_REMOVE)) {
            m_pMenu->addAction(m_pRemoveAct);
        }
        if (modelHasCapabilities(TrackModel::TRACKMODELCAPS_RELOCATE)) {
            m_pMenu->addAction(m_pRelocate);
        }
        if (modelHasCapabilities(TrackModel::TRACKMODELCAPS_RELOADMETADATA)) {
            m_pMenu->addAction(m_pReloadMetadataAct);
        }
        m_pPropertiesAct->setEnabled(oneSongSelected);
        m_pMenu->addAction(m_pPropertiesAct);

        //Create the right-click menu
        m_pMenu->popup(event->globalPos());


        m_libraryViewContainer->filter()->SetLibraryModel(library->model());
        //m_libraryViewContainer->filter()->SetSettingsGroup(kSettingsGroup);
         *
         *
         */
}


TrackPointer ClementineView::getTrack(const Song& song) {
    QString location;

    location = song.url().toLocalFile();

    if (location.isEmpty()) {
        return TrackPointer();
    }

    TrackDAO& track_dao = m_pTrackCollection->getTrackDAO();
    int track_id = track_dao.getTrackId(location);
    bool track_already_in_library = (track_id >= 0);
    if (track_id < 0) {
        // Add Track to library
        track_id = track_dao.addTrack(location, true);
    }

    TrackPointer pTrack;

    if (track_id < 0) {
        // Add Track to library failed, create a transient TrackInfoObject
        pTrack = TrackPointer(new TrackInfoObject(location), &QObject::deleteLater);
    } else {
        pTrack = track_dao.getTrack(track_id);
    }

    // If this track was not in the Mixxx library it is now added and will be
    // saved with the metadata from iTunes. If it was already in the library
    // then we do not touch it so that we do not over-write the user's metadata.
    if (!track_already_in_library) {
        pTrack->setArtist(song.artist());
        pTrack->setTitle(song.title());
        pTrack->setDuration(song.length_nanosec());
        pTrack->setAlbum(song.album());
        pTrack->setYear(QString::number(song.year()));
        pTrack->setRating((int)song.rating());
        pTrack->setGenre(song.genre());
        pTrack->setTrackNumber(QString::number(song.track()));
        float bpm = song.bpm();
        pTrack->setBpm(bpm);
        pTrack->setBitrate(song.bitrate());
        pTrack->setComment(song.comment());
        pTrack->setComposer(song.composer());
        // If the track has a BPM, then give it a static beatgrid.
        if (bpm) {
            BeatsPointer pBeats = BeatFactory::makeBeatGrid(pTrack, bpm, 0);
            pTrack->setBeats(pBeats);
        }
    }
    return pTrack;
}

TrackPointer ClementineView::getTrack(const QUrl& url) {
    QString location;

    location = url.toLocalFile();

    if (location.isEmpty()) {
        return TrackPointer();
    }

    TrackDAO& track_dao = m_pTrackCollection->getTrackDAO();
    int track_id = track_dao.getTrackId(location);
    if (track_id < 0) {
        // Add Track to library
        track_id = track_dao.addTrack(location, true);
    }

    TrackPointer pTrack;

    if (track_id < 0) {
        // Add Track to library failed, create a transient TrackInfoObject
        pTrack = TrackPointer(new TrackInfoObject(location), &QObject::deleteLater);
    } else {
        pTrack = track_dao.getTrack(track_id);
    }
    return pTrack;
}
