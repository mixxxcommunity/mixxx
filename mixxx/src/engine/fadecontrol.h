#ifndef FADECONTROL_H
#define FADECONTROL_H

#include <QMutex>

#include "engine/enginecontrol.h"
#include "configobject.h"
#include "trackinfoobject.h"

class FadeControl : public EngineControl {
    Q_OBJECT
public:
    FadeControl(const char * _group,
                ConfigObject<ConfigValue> * _config);
    virtual ~FadeControl();

public slots:
    void loadTrack(TrackPointer pTrack);
    void unloadTrack(TrackPointer pTrack);

private slots:
    void slotFadeInSet(double v);
    void slotFadeOutSet(double v);

private:
    ControlObject* m_pCOTrackSamples;
    ControlObject* m_pQuantizeEnabled;
    ControlObject* m_pNextBeat;

    ControlObject* m_pCOFadeInPosition;
    ControlObject* m_pCOFadeOutPosition;

    // FadePoint button controls
    ControlObject* m_pFadepointInSet;
    ControlObject* m_pFadepointOutSet;

    TrackPointer m_pLoadedTrack;

    QMutex m_mutex;
};

#endif // FADECONTROL_H
