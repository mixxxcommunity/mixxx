// clementineview.cpp
// Created 2/25/2011 by Daniel Schürmann (daschuer@gmx.de)
//
// This shows the Clementine-Player library in Mixxx



#include "clementineview.h"
#include "clementinefeature.h"
#include "trackinfoobject.h"
#include "track/beatfactory.h"
#include "controlobjectthreadmain.h"
#include "controlobject.h"

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
#include <QSignalMapper>
#include <QAction>
#include <QMenu>
#include <QHBoxLayout>

ClementineView::ClementineView(QWidget* parent, ConfigObject<ConfigValue>* pConfig, TrackCollection* pTrackCollection)
    : QWidget(parent),
      m_libraryViewContainer(new LibraryViewContainer(this)),
      m_librarySortModel(new QSortFilterProxyModel(this)),
      m_pConfig(pConfig),
      m_pData(NULL),
      m_pTrackCollection(pTrackCollection) {
    m_horizontalLayout = new QHBoxLayout(this);
    m_horizontalLayout->setContentsMargins(0, 0, 0, 0);
    m_horizontalLayout->addWidget(m_libraryViewContainer);
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

    // Is emmited also for doubleClick
    connect(m_libraryViewContainer->view(), SIGNAL(AddToPlaylistSignal(QMimeData*)),
            SLOT(slotAddToClementinePlaylist(QMimeData*)));

    connect(m_libraryViewContainer->view(), SIGNAL(RightClicked(QMimeData*)),
            SLOT(slotLibraryViewRightClicked(QMimeData*)));

    m_libraryViewContainer->view()->AddSongActionSeparator();

    // Auto DJ Actions
    m_libraryViewContainer->view()->AddSongAction(QIcon(),
            tr("Add to Auto DJ bottom"), this, SLOT(slotSendToAutoDJ()));
    m_libraryViewContainer->view()->AddSongAction(QIcon(),
            tr("Add to Auto DJ top 2"), this, SLOT(slotSendToAutoDJTop()));

    m_libraryViewContainer->view()->AddSongActionSeparator();


    connect(&m_groupMapper, SIGNAL(mapped(QString)),
            SLOT(loadSelectionToGroup(QString)));

    ControlObjectThreadMain* pNumDecks = new ControlObjectThreadMain(
            ControlObject::getControl(ConfigKey("[Master]", "num_decks")));
    int iNumDecks = pNumDecks->get();
    delete pNumDecks;

    // Load to Deck Actions
    for (int i = 1; i <= iNumDecks; ++i) {
        QString deckGroup = QString("[Channel%1]").arg(i);
        QAction* pAction = m_libraryViewContainer->view()->AddSongAction(QIcon(),
                tr("Load to Deck %1").arg(i), &m_groupMapper, SLOT(map()));
        m_groupMapper.setMapping(pAction, deckGroup);
    }


    ControlObjectThreadMain* pNumSamplers = new ControlObjectThreadMain(
            ControlObject::getControl(ConfigKey("[Master]", "num_samplers")));
    int iNumSamplers = pNumSamplers->get();
    delete pNumSamplers;

    if (iNumSamplers > 0) {
        m_pMenuSampler = m_libraryViewContainer->view()->AddSongActionMenu();
        m_pMenuSampler->setTitle(tr("Load to Sampler"));
        for (int i = 1; i <= iNumSamplers; ++i) {
            QString samplerGroup = QString("[Sampler%1]").arg(i);
            QAction* pAction = new QAction(tr("Sampler %1").arg(i), m_pMenuSampler);
            m_pMenuSampler->addAction(pAction);
            m_groupMapper.setMapping(pAction, samplerGroup);
            connect(pAction, SIGNAL(triggered()), &m_groupMapper, SLOT(map()));
        }
    }

    m_libraryViewContainer->view()->AddSongActionSeparator();

    connect(&m_playlistMapper, SIGNAL(mapped(int)),
            this, SLOT(slotAddToMixxxPlaylist(int)));

    m_pMenuPlaylist = m_libraryViewContainer->view()->AddSongActionMenu();
    m_pMenuPlaylist->setTitle(tr("Add to Playlist"));

    connect(&m_crateMapper, SIGNAL(mapped(int)),
            this, SLOT(addSelectionToCrate(int)));

    m_pMenuCrate = m_libraryViewContainer->view()->AddSongActionMenu();
    m_pMenuCrate->setTitle(tr("Add to Crate"));



    m_libraryViewContainer->filter()->SetLibraryModel(library->model());
    //m_libraryViewContainer->filter()->SetSettingsGroup(kSettingsGroup);
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

void ClementineView::addToMixxPlaylist(int iPlaylistId, bool bTop) {

    PlaylistDAO& playlistDao = m_pTrackCollection->getPlaylistDAO();

    if (iPlaylistId <= 0 || !m_pData) {
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
                    playlistDao.insertTrackIntoPlaylist(iTrackId, iPlaylistId, 2);
                }
                else {
                    playlistDao.appendTrackToPlaylist(iTrackId, iPlaylistId);
                }
            }
        }
    //} else if (const GeneratorMimeData* generator_data = qobject_cast<const GeneratorMimeData*>(data)) {
    //    InsertSmartPlaylist(generator_data->generator_, row, play_now, enqueue_now);
    //} else if (const PlaylistItemMimeData* item_data = qobject_cast<const PlaylistItemMimeData*>(data)) {
    //    InsertItems(item_data->items_, row, play_now, enqueue_now);
    } else if (m_pData->hasUrls()) {
        foreach (QUrl url, m_pData->urls()) {
            qDebug() << "-----" << url;
            TrackPointer pTrack = getTrack(url);
            int iTrackId = pTrack->getId();
            if (iTrackId != -1) {
                if (bTop) {
                    // Load track to position two because position one is already loaded to the player
                    playlistDao.insertTrackIntoPlaylist(iTrackId, iPlaylistId, 2);
                }
                else {
                    playlistDao.appendTrackToPlaylist(iTrackId, iPlaylistId);
                }
            }
        }
    }
    delete m_pData;
}

void ClementineView::slotSendToAutoDJ() {
    // append to auto DJ
    PlaylistDAO& playlistDao = m_pTrackCollection->getPlaylistDAO();
    addToMixxPlaylist(playlistDao.getPlaylistIdFromName(AUTODJ_TABLE), false); // bTop = false
}

void ClementineView::slotSendToAutoDJTop() {
    PlaylistDAO& playlistDao = m_pTrackCollection->getPlaylistDAO();
    addToMixxPlaylist(playlistDao.getPlaylistIdFromName(AUTODJ_TABLE), true); // bTop = true
}

void ClementineView::slotAddToMixxxPlaylist(int iPlaylistId) {
    addToMixxPlaylist(iPlaylistId, false); // bTop = false
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
    if (!m_pData) {
        return;
    }

    if ( !(m_pConfig->getValueString(ConfigKey("[Controls]","AllowTrackLoadToPlayingDeck")).toInt()) ) {
        bool groupPlaying = (ControlObject::getControl(
                ConfigKey(group, "play"))->get() == 1.0f);

        if (groupPlaying) {
            return;
        }
    }

    TrackPointer pTrack;

    if (const SongMimeData* songData = qobject_cast<const SongMimeData*>(m_pData)) {
        foreach (Song song, songData->songs) {
            qDebug() << "-----" << song.title();
            pTrack = getTrack(song);
            break;
        }
    } else if (m_pData->hasUrls()) {
        foreach (QUrl url, m_pData->urls()) {
            qDebug() << "-----" << url;
            pTrack = getTrack(url);
            break;
        }
    }

    if (pTrack) {
        emit(loadTrackToPlayer(pTrack, group));
    }

    delete m_pData;
}

void ClementineView::slotLibraryViewRightClicked(QMimeData* data) {
    // setup Context Menu actions
    qDebug() << "slotLibraryViewRightClicked";

    m_pData = data;

    bool oneSongSelected = false;
    if (data) {
        if (data->hasUrls()) {
            if (data->urls().count() == 1) {
                oneSongSelected = true;
            }
        }
    }

    ControlObjectThreadMain* pNumDecks = new ControlObjectThreadMain(
            ControlObject::getControl(ConfigKey("[Master]", "num_decks")));
    int iNumDecks = pNumDecks->get();
    delete pNumDecks;

    // Load to Deck Actions
    for (int i = 1; i <= iNumDecks; ++i) {
        QString deckGroup = QString("[Channel%1]").arg(i);
        QAction* pAction = (QAction*)m_groupMapper.mapping(deckGroup);
        if (pAction) {
            bool deckPlaying = ControlObject::getControl(
                    ConfigKey(deckGroup, "play"))->get() == 1.0f;
            pAction->setEnabled(!deckPlaying && oneSongSelected);
        }
    }

    ControlObjectThreadMain* pNumSamplers = new ControlObjectThreadMain(
            ControlObject::getControl(ConfigKey("[Master]", "num_samplers")));
    int iNumSamplers = pNumSamplers->get();
    delete pNumSamplers;

    for (int i = 1; i <= iNumSamplers; ++i) {
        QString samplerGroup = QString("[Sampler%1]").arg(i);
        QAction* pAction = (QAction*)m_groupMapper.mapping(samplerGroup);
        if (pAction) {
            bool samplerPlaying = ControlObject::getControl(
                    ConfigKey(samplerGroup, "play"))->get() == 1.0f;
            pAction->setEnabled(!samplerPlaying && oneSongSelected);
        }
    }

    m_pMenuSampler->setEnabled(oneSongSelected);

    m_pMenuPlaylist->clear();
    PlaylistDAO& playlistDao = m_pTrackCollection->getPlaylistDAO();
    int numPlaylists = playlistDao.playlistCount();

    if (numPlaylists) {
        for (int i = 0; i < numPlaylists; ++i) {
            int iPlaylistId = playlistDao.getPlaylistId(i);

            if (!playlistDao.isHidden(iPlaylistId)) {
                QAction* pAction = new QAction(playlistDao.getPlaylistName(iPlaylistId), m_pMenuPlaylist);
                bool locked = playlistDao.isPlaylistLocked(iPlaylistId);
                pAction->setEnabled(!locked);
                m_pMenuPlaylist->addAction(pAction);
                m_playlistMapper.setMapping(pAction, iPlaylistId);
                connect(pAction, SIGNAL(triggered()), &m_playlistMapper, SLOT(map()));
            }
        }
    } else {
        m_pMenuPlaylist->setVisible(false);
        m_pMenuPlaylist->menuAction()->setEnabled(false);
    }

    m_pMenuCrate->clear();
    CrateDAO& crateDao = m_pTrackCollection->getCrateDAO();
    int numCrates = crateDao.crateCount();

    if (numCrates) {
        for (int i = 0; i < numCrates; ++i) {
            int iCrateId = crateDao.getCrateId(i);
            QAction* pAction = new QAction(crateDao.crateName(iCrateId), m_pMenuCrate);
            pAction->setEnabled(!crateDao.isCrateLocked(iCrateId));
            m_pMenuCrate->addAction(pAction);
            m_crateMapper.setMapping(pAction, iCrateId);
            connect(pAction, SIGNAL(triggered()), &m_crateMapper, SLOT(map()));
        }
    } else {
        m_pMenuCrate->setVisible(false);
        m_pMenuCrate->menuAction()->setEnabled(false);
    }
}

void ClementineView::slotAddToClementinePlaylist(QMimeData* data) {
    if (!data) {
        return;
    }

    if (const SongMimeData* songData = qobject_cast<const SongMimeData*>(data)) {
        // Should we replace the flags with the user's preference?
        if (songData->override_user_settings_) {
            // Do nothing
        } else if (songData->from_doubleclick_) {
            TrackPointer pTrack;
            foreach (Song song, songData->songs) {
                qDebug() << "-----" << song.title();
                pTrack = getTrack(song);
                break;
            }
            if (pTrack) {
                emit(loadTrack(pTrack));
            }
            //ApplyAddBehaviour(doubleclick_addmode_, mime_data);
            //ApplyPlayBehaviour(doubleclick_playmode_, mime_data);
        } else {
            //ApplyPlayBehaviour(menu_playmode_, mime_data);
        }

        // Should we create a new playlist for the songs?
        //if(mime_data->open_in_new_playlist_) {
            //playlists_->New(mime_data->get_name_for_new_playlist());
        //}
    }

    //playlists_->current()->dropMimeData(data, Qt::CopyAction, -1, 0, QModelIndex());
    delete data;
}


void ClementineView::addSelectionToCrate(int iCrateId) {
    CrateDAO& crateDao = m_pTrackCollection->getCrateDAO();

    if (iCrateId <= 0 || !m_pData) {
        return;
    }

    if (const SongMimeData* songData = qobject_cast<const SongMimeData*>(m_pData)) {
        foreach (Song song, songData->songs) {
            qDebug() << "-----" << song.title();
            TrackPointer pTrack = getTrack(song);
            int iTrackId = pTrack->getId();
            if (iTrackId != -1) {
                crateDao.addTrackToCrate(iTrackId, iCrateId);
            }
        }
    //} else if (const GeneratorMimeData* generator_data = qobject_cast<const GeneratorMimeData*>(data)) {
    //    InsertSmartPlaylist(generator_data->generator_, row, play_now, enqueue_now);
    //} else if (const PlaylistItemMimeData* item_data = qobject_cast<const PlaylistItemMimeData*>(data)) {
    //    InsertItems(item_data->items_, row, play_now, enqueue_now);
    } else if (m_pData->hasUrls()) {
        foreach (QUrl url, m_pData->urls()) {
            qDebug() << "-----" << url;
            TrackPointer pTrack = getTrack(url);
            int iTrackId = pTrack->getId();
            if (iTrackId != -1) {
                crateDao.addTrackToCrate(iTrackId, iCrateId);
            }
        }
    }
    delete m_pData;
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


