// autodjfeature.cpp
// FORK FORK FORK on 11/1/2009 by Albert Santoni (alberts@mixxx.org)
// Created 8/23/2009 by RJ Ryan (rryan@mit.edu)

#include <QtDebug>

#include "library/autodjfeature.h"
#include "library/playlisttablemodel.h"
#include "library/trackcollection.h"
#include "dlgautodj.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarysidebar.h"
#include "mixxxkeyboard.h"
#include "soundsourceproxy.h"

const QString AutoDJFeature::m_sAutoDJViewName = QString("Auto DJ");

AutoDJFeature::AutoDJFeature(QObject* parent,
                             ConfigObject<ConfigValue>* pConfig,
                             TrackCollection* pTrackCollection)
        : LibraryFeature(parent),
          m_pConfig(pConfig),
          m_pTrackCollection(pTrackCollection),
          m_playlistDao(pTrackCollection->getPlaylistDAO()) {
    m_pAutoDJView = NULL;
}

AutoDJFeature::~AutoDJFeature() {
}

QVariant AutoDJFeature::title() {
    return tr("Auto DJ");
}

QIcon AutoDJFeature::getIcon() {
    return QIcon(":/images/library/ic_library_autodj.png");
}

void AutoDJFeature::bindWidget(WLibrarySidebar* /*sidebarWidget*/,
                               WLibrary* libraryWidget,
                               MixxxKeyboard* keyboard) {

    m_pAutoDJView = new DlgAutoDJ(libraryWidget,
                                  m_pConfig,
                                  m_pTrackCollection,
                                  keyboard);
    m_pAutoDJView->installEventFilter(keyboard);
    libraryWidget->registerView(m_sAutoDJViewName, m_pAutoDJView);

    // TODO(tom__m) This is wrong to create this here, fix the COs returning NULL when creating in constructor
    m_pAutoDJ = new AutoDJ(this, m_pConfig);

    connect(m_pAutoDJ, SIGNAL(loadTrack(TrackPointer)),
            this, SIGNAL(loadTrack(TrackPointer)));
    connect(m_pAutoDJ, SIGNAL(loadTrackToPlayer(TrackPointer,QString)),
            this, SIGNAL(loadTrackToPlayer(TrackPointer,QString)));
    connect(m_pAutoDJView, SIGNAL(loadTrackToPlayer(TrackPointer, QString)),
            this, SIGNAL(loadTrackToPlayer(TrackPointer, QString)));

    // Connect the signals to and from the AutoDJ object
    connect(m_pAutoDJView, SIGNAL(setAutoDJEnabled(bool)),
            m_pAutoDJ, SLOT(setEnabled(bool)));
    connect(m_pAutoDJView, SIGNAL(sendNextTrack(TrackPointer)),
            m_pAutoDJ, SLOT(receiveNextTrack(TrackPointer)));
    connect(m_pAutoDJView, SIGNAL(endOfPlaylist(bool)),
            m_pAutoDJ, SLOT(setEndOfPlaylist(bool)));
    connect(m_pAutoDJ, SIGNAL(needNextTrack()),
            m_pAutoDJView, SLOT(slotNextTrackNeeded()));
    connect(m_pAutoDJ, SIGNAL(disableAutoDJ()),
            m_pAutoDJView, SLOT(slotDisableAutoDJ()));
    connect(m_pAutoDJ, SIGNAL(removePlayingTrackFromQueue(QString)),
            m_pAutoDJView, SLOT(slotRemovePlayingTrackFromQueue(QString)));
}

TreeItemModel* AutoDJFeature::getChildModel() {
    return &m_childModel;
}

void AutoDJFeature::activate() {
    //qDebug() << "AutoDJFeature::activate()";
    //emit(showTrackModel(m_pAutoDJTableModelProxy));
    emit(switchToView(m_sAutoDJViewName));
}

void AutoDJFeature::activateChild(const QModelIndex& /*index*/) {
}

void AutoDJFeature::onRightClick(const QPoint& /*globalPos*/) {
}

void AutoDJFeature::onRightClickChild(const QPoint& /*globalPos*/,
                                      QModelIndex /*index*/) {
}

bool AutoDJFeature::dropAccept(QUrl url) {
    //TODO: Filter by supported formats regex and reject anything that doesn't match.
    TrackDAO &trackDao = m_pTrackCollection->getTrackDAO();

    //If a track is dropped onto a playlist's name, but the track isn't in the library,
    //then add the track to the library before adding it to the playlist.

    //XXX: See the note in PlaylistFeature::dropAccept() about using QUrl::toLocalFile()
    //     instead of toString()
    QFileInfo file(url.toLocalFile());

    if (!SoundSourceProxy::isFilenameSupported(file.fileName())) {
        return false;
    }

    // Adds track, does not insert duplicates, handles unremoving logic.
    int trackId = trackDao.addTrack(file, true);

    if (trackId < 0) {
        return false;
    }

    // TODO(XXX) No feedback on whether this worked.
    if (m_pAutoDJView) {
        m_pAutoDJView->appendTrack(trackId);
    } else {
        int playlistId = m_playlistDao.getPlaylistIdFromName(AUTODJ_TABLE);
        m_playlistDao.appendTrackToPlaylist(trackId, playlistId);
    }

    return true;
}

bool AutoDJFeature::dropAcceptChild(const QModelIndex& /*index*/, QUrl /*url*/) {
    return false;
}

bool AutoDJFeature::dragMoveAccept(QUrl url) {
    QFileInfo file(url.toLocalFile());
    return SoundSourceProxy::isFilenameSupported(file.fileName());
}

bool AutoDJFeature::dragMoveAcceptChild(const QModelIndex& /*index*/,
                                        QUrl /*url*/) {
    return false;
}
void AutoDJFeature::onLazyChildExpandation(const QModelIndex& /*index*/){
    //Nothing to do because the childmodel is not of lazy nature.
}
