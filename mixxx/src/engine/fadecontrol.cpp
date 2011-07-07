#include <QMutexLocker>

#include "engine/fadecontrol.h"

#include "controlobject.h"

FadeControl::FadeControl(const char *_group,
                         ConfigObject<ConfigValue> *_config) :
        EngineControl(_group, _config),
        m_mutex(QMutex::Recursive) {
    m_pTrackSamples = ControlObject::getControl(ConfigKey(_group, "track_samples"));
}

FadeControl::~FadeControl() {
    delete m_pTrackSamples;
}

void FadeControl::loadTrack(TrackPointer pTrack) {
    Q_ASSERT(pTrack);

    QMutexLocker lock(&m_mutex);
    if(m_pLoadedTrack)
        unloadTrack(m_pLoadedTrack);

    m_pLoadedTrack = pTrack;
    //FadesUpdated?? connect

    // If this is the first time loaded, default the fade points to 5% from the
    // beginning and end of the track
    if(pTrack->getFadeIn() == NULL) {
        int samples = m_pTrackSamples->get();
        samples *= 0.05;
        if (samples % 2 != 0) {
            samples--;
        }
        pTrack->setFadeIn(samples);
    }
    if(pTrack->getFadeOut() == NULL) {
        int samples = m_pTrackSamples->get();
        samples *= 0.95;
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
