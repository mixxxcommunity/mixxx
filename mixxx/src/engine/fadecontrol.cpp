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

    m_pQuantizeEnabled = ControlObject::getControl(ConfigKey(_group, "quantize"));
    m_pNextBeat = ControlObject::getControl(ConfigKey(_group, "beat_next"));

    m_pFadepointInSet = new ControlPushButton(ConfigKey(_group, "fadein_set"));
    connect(m_pFadepointInSet, SIGNAL(valueChanged(double)),
            this, SLOT(slotFadeInSet(double)));

    m_pFadepointOutSet = new ControlPushButton(ConfigKey(_group, "fadeout_set"));
    connect(m_pFadepointOutSet, SIGNAL(valueChanged(double)),
            this, SLOT(slotFadeOutSet(double)));

}

FadeControl::~FadeControl() {
    delete m_pFadepointInSet;
    delete m_pFadepointOutSet;
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
    if (!v) {
        return;
    }

    QMutexLocker lock(&m_mutex);
    double fadeIn = m_pQuantizeEnabled->get() ?
                math_max(0., floorf(m_pNextBeat->get())) :
                math_max(0., floorf(getCurrentSample()));
    if (!even(fadeIn)) {
        fadeIn--;
    }

    if (m_pLoadedTrack) {
        if (m_pLoadedTrack->getFadeOut() < fadeIn) {
            qDebug() << "Cannot set the FadeIn point after the FadeOut point";
            return;
        }

        qDebug() << "Setting FadeIn point as: " << fadeIn;
        m_pLoadedTrack->setFadeIn(fadeIn);
    }
}

void FadeControl::slotFadeOutSet(double v) {
    if (!v) {
        return;
    }

    QMutexLocker lock(&m_mutex);
    double fadeOut = m_pQuantizeEnabled->get() ?
                math_max(0., floorf(m_pNextBeat->get())) :
                math_max(0., floorf(getCurrentSample()));
    if (!even(fadeOut)) {
        fadeOut--;
    }

    if (m_pLoadedTrack) {
        if (m_pLoadedTrack->getFadeIn() > fadeOut) {
            qDebug() << "Cannot set the FadeOut point before the FadeIn point";
            return;
        }

        qDebug() << "Setting FadeOut point as: " << fadeOut;
        m_pLoadedTrack->setFadeOut(fadeOut);
    }
}
