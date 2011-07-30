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

private:
    ControlObject* m_pCOTrackSamples;

    TrackPointer m_pLoadedTrack;

    QMutex m_mutex;
};

#endif // FADECONTROL_H
