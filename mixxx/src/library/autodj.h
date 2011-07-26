#ifndef AUTODJ_H
#define AUTODJ_H

#include <QObject>

#include "dlgautodj.h"
#include "library/tracktransition.h"

class ControlObjectThreadMain;

class AutoDJ : public QObject {
    Q_OBJECT

public:
    AutoDJ(QObject* parent, ConfigObject<ConfigValue>* pConfig);
    ~AutoDJ();

public slots:
    void setEnabled(bool);
    void setEndOfPlaylist(bool);
    void receiveNextTrack(TrackPointer nextTrack);
    void player1PositionChanged(double samplePos1);
    void player2PositionChanged(double samplePos2);

signals:
    void needNextTrack();
    void disableAutoDJ();
    void loadTrack(TrackPointer tio);
    void loadTrackToPlayer(TrackPointer tio, QString group);

private:
    bool m_bEnabled;
    bool m_bEndOfPlaylist;
    bool m_bNextTrackAlreadyLoaded; /** Makes our Auto DJ logic assume the
                                        next track that should be played is
                                        already loaded. We need this flag to
                                        make our first-track-gets-loaded-but-
                                        not-removed-from-the-queue behaviour
                                        work. */
    bool m_bPlayer1Primed, m_bPlayer2Primed;
    // TODO(tom__m) This is hacky
    bool m_bPlayer1Cued, m_bPlayer2Cued;

    ConfigObject<ConfigValue>* m_pConfig;
    ControlObjectThreadMain* m_pCOPlayPosSamples1;
    ControlObjectThreadMain* m_pCOPlayPosSamples2;
    ControlObjectThreadMain* m_pCOSampleRate1;
    ControlObjectThreadMain* m_pCOSampleRate2;
    ControlObjectThreadMain* m_pCOPlay1;
    ControlObjectThreadMain* m_pCOPlay2;
    ControlObjectThreadMain* m_pCORepeat1;
    ControlObjectThreadMain* m_pCORepeat2;
    ControlObjectThreadMain* m_pCOCrossfader;
    TrackPointer m_pNextTrack;
    TrackTransition* m_pTrackTransition;

    // Handles the emission of signals needed to load the next track as well as
    // signal that AutoDJ need a new track
    void loadNextTrack();

    // Cues the track in the specified player to the X seconds before the FadeIn point
    // where X is the user defined crossfade length
    void cueTrackToFadeIn(QString group);

};

#endif // AUTODJ_H
