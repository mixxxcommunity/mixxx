#include <QMutexLocker>

#include "engine/fadecontrol.h"

#include "controlobject.h"
#include "controlpushbutton.h"
#include "mathstuff.h"

FadeControl::FadeControl(const char *_group,
                         ConfigObject<ConfigValue> *_config) :
        EngineControl(_group, _config),
        m_mutex(QMutex::Recursive) {

    m_pCOTrackSamples = ControlObject::getControl(ConfigKey(_group, "track_samples"));

    m_pFadepointInSet = new ControlPushButton(ConfigKey(_group, "fadein_set"));
    connect(m_pFadepointInSet, SIGNAL(valueChanged(double)),
            this, SLOT(slotFadeInSet(double)));

    m_pFadepointOutSet = new ControlPushButton(ConfigKey(_group, "fadeout_set"));
    connect(m_pFadepointOutSet, SIGNAL(valueChanged(double)),
            this, SLOT(slotFadeOutSet(double)));

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
        double samples = trackSamples * 0.05;
        if (!even(samples)) {
            samples--;
        }
        if ((cuePoint + samples) < trackSamples) {
            samples = cuePoint + samples;
        }
        pTrack->setFadeIn(samples);
    }
    if(!pTrack->getFadeOut()) {
        double samples = trackSamples * 0.95;
        if (!even(samples)) {
            samples--;
        }
        pTrack->setFadeOut(samples);
    }
}

void FadeControl::unloadTrack(TrackPointer pTrack) {
    QMutexLocker lock(&m_mutex);
    m_pLoadedTrack.clear();
}

void FadeControl::slotFadeInSet(double v) {
}

void FadeControl::slotFadeOutSet(double v) {
}
