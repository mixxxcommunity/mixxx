#ifndef AUTODJ_H
#define AUTODJ_H

#include <QObject>

#include "dlgautodj.h"
#include "configobject.h"

class ControlObjectThreadMain;

class AutoDJ : public QObject {
    Q_OBJECT

public:
    AutoDJ(QObject* parent);
    ~AutoDJ();

public slots:
    void setEnabled(bool);
    void setEndOfPlaylist(bool);
    void receiveNextTrack(TrackPointer nextTrack);
    void player1PositionChanged(double value);
    void player2PositionChanged(double value);

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

    ControlObjectThreadMain* m_pCOPlayPos1;
    ControlObjectThreadMain* m_pCOPlayPos2;
    ControlObjectThreadMain* m_pCOPlay1;
    ControlObjectThreadMain* m_pCOPlay2;
    ControlObjectThreadMain* m_pCORepeat1;
    ControlObjectThreadMain* m_pCORepeat2;
    ControlObjectThreadMain* m_pCOCrossfader;
    TrackPointer m_pNextTrack;
    void loadNextTrack();


};

#endif // AUTODJ_H
