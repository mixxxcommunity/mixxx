#include <QMutexLocker>

#include "engine/fadecontrol.h"

#include "controlobject.h"

FadeControl::FadeControl(const char *_group,
                         ConfigObject<ConfigValue> *_config) :
        EngineControl(_group, _config),
        m_mutex(QMutex::Recursive) {
    m_pCOTrackSamples = ControlObject::getControl(ConfigKey(_group, "track_samples"));
}

FadeControl::~FadeControl() {
}

void FadeControl::loadTrack(TrackPointer pTrack) {
    Q_ASSERT(pTrack);

    QMutexLocker lock(&m_mutex);
    if(m_pLoadedTrack)
        unloadTrack(m_pLoadedTrack);

    m_pLoadedTrack = pTrack;
    const double trackSamples = m_pCOTrackSamples->get();
    //FadesUpdated?? connect

    // If this is the first time loaded, default the fadeIn point to 5% of the track's
    // length after the CuePoint and the fadeOut point to 5% before the end of the track.
    if(!pTrack->getFadeIn()) {
        float cuePoint = pTrack->getCuePoint();
        int samples = trackSamples * 0.05;
        if (samples % 2 != 0) {
            samples--;
        }
        if ((cuePoint + samples) < trackSamples) {
            samples = cuePoint + samples;
        }
        pTrack->setFadeIn(samples);
    }
    if(!pTrack->getFadeOut()) {
        int samples = trackSamples * 0.95;
        if (samples % 2 != 0) {
            samples--;
        }
        pTrack->setFadeOut(samples);
    }
}

void FadeControl::unloadTrack(TrackPointer pTrack) {
    QMutexLocker lock(&m_mutex);
    m_pLoadedTrack.clear();
}
