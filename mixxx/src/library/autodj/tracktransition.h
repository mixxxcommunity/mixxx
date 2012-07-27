#ifndef TRACKTRANSITION_H
#define TRACKTRANSITION_H

#include <QObject>

#include "configobject.h"
#include "trackinfoobject.h"
#include "library/autodj/autodj.h"

class ControlObjectThreadMain;

class TrackTransition : public QObject {
    Q_OBJECT

public:
    TrackTransition(QObject* parent, ConfigObject<ConfigValue>* pConfig);
    virtual ~TrackTransition();
    void setGroups(QString groupA, QString groupB);
    void calculateCue();

public slots:
	void crossfaderChange(double value);

signals:
	void transitionDone();

private:
	double m_dcrossfadePosition;
    double m_dCurrentPlayPos;
    bool m_bUserTakeOver;
    bool m_bTransitioning;
    bool m_bShortCue;
    bool m_bFadeNow;
    //bool m_bPastCue;
    QString m_groupA;
    QString m_groupB;
    TrackPointer m_trackA;
    TrackPointer m_trackB;
    //int m_iTrackACue;
    int m_iCurrentPos;
    int m_iShortCue;
    int m_iCuePoint;
    int m_iEndPoint;
    int m_iFadeStart;
    int m_iFadeEnd;
    int m_iFadeLength;
    double m_dCrossfaderStart;
    bool transitioning();
    void calculateShortCue();
    //int m_iTrackBCue;
    // This is the transition function used by AutoDJ to crossfade from the desired
    // deckA to deckB
    void cueTransition(double value);
    void cdTransition(double value);
    void beatTransition(double value);

    void fadeNowRight();
    void fadeNowLeft();

    ConfigObject<ConfigValue>* m_pConfig;
    ControlObjectThreadMain* m_pCOCrossfader;
    ControlObjectThreadMain* m_pCOPlayPos1;
    ControlObjectThreadMain* m_pCOPlayPos2;
    ControlObjectThreadMain* m_pCOPlay1;
    ControlObjectThreadMain* m_pCOPlay2;
    ControlObjectThreadMain* m_pCOTrackSamples1;
    ControlObjectThreadMain* m_pCOTrackSamples2;
    ControlObjectThreadMain* m_pCOSync1;
    ControlObjectThreadMain* m_pCOSync2;
    ControlObjectThreadMain* m_pCOCueOut1;
    ControlObjectThreadMain* m_pCOCueOut2;

    friend class AutoDJ;

};

#endif // TRACKTRANSITION_H
