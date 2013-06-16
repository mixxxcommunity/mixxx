// bpmcontrol.cpp
// Created 7/5/2009 by RJ Ryan (rryan@mit.edu)

#include <QtDebug>

#include "controlobject.h"
#include "controlpushbutton.h"

#include "engine/enginebuffer.h"
#include "engine/enginestate.h"
#include "engine/bpmcontrol.h"

const int minBpm = 30;
const int maxInterval = (int)(1000.*(60./(CSAMPLE)minBpm));
const int filterLength = 5;

BpmControl::BpmControl(const char* _group,
                       ConfigObject<ConfigValue>* _config,
                       EngineState* pEngineState) :
        EngineControl(_group, _config),
        m_tapFilter(this, filterLength, maxInterval) {
    m_pTrackWatcher = pEngineState->getTrackManager()->createTrackWatcher();
    connect(m_pTrackWatcher, SIGNAL(beatsUpdated()),
            this, SLOT(slotUpdatedTrackBeats()),
            Qt::DirectConnection);

    CallbackControlManager* pControlManager = pEngineState->getControlManager();
    m_pPlayButton = pControlManager->getControl(ConfigKey(_group, "play"));
    m_pRateSlider = pControlManager->getControl(ConfigKey(_group, "rate"));
    connect(m_pRateSlider, SIGNAL(valueChanged(double)),
            this, SLOT(slotRateChanged(double)),
            Qt::DirectConnection);
    connect(m_pRateSlider, SIGNAL(valueChangedFromEngine(double)),
            this, SLOT(slotRateChanged(double)),
            Qt::DirectConnection);

    m_pRateRange = pControlManager->getControl(ConfigKey(_group, "rateRange"));
    connect(m_pRateRange, SIGNAL(valueChanged(double)),
            this, SLOT(slotRateChanged(double)),
            Qt::DirectConnection);
    connect(m_pRateRange, SIGNAL(valueChangedFromEngine(double)),
            this, SLOT(slotRateChanged(double)),
            Qt::DirectConnection);

    m_pRateDir = pControlManager->getControl(ConfigKey(_group, "rate_dir"));
    connect(m_pRateDir, SIGNAL(valueChanged(double)),
            this, SLOT(slotRateChanged(double)),
            Qt::DirectConnection);
    connect(m_pRateDir, SIGNAL(valueChangedFromEngine(double)),
            this, SLOT(slotRateChanged(double)),
            Qt::DirectConnection);

    m_pFileBpm = pControlManager->addControl(
        new ControlObject(ConfigKey(_group, "file_bpm")), 1);
    connect(m_pFileBpm, SIGNAL(valueChanged(double)),
            this, SLOT(slotFileBpmChanged(double)),
            Qt::DirectConnection);

    m_pEngineBpm = pControlManager->addControl(
        new ControlObject(ConfigKey(_group, "bpm")), 1);
    connect(m_pEngineBpm, SIGNAL(valueChanged(double)),
            this, SLOT(slotSetEngineBpm(double)),
            Qt::DirectConnection);

    m_pButtonTap = pControlManager->addControl(
        new ControlPushButton(ConfigKey(_group, "bpm_tap")), 1);
    connect(m_pButtonTap, SIGNAL(valueChanged(double)),
            this, SLOT(slotBpmTap(double)),
            Qt::DirectConnection);

    // Beat sync (scale buffer tempo relative to tempo of other buffer)
    m_pButtonSync = pControlManager->addControl(
        new ControlPushButton(ConfigKey(_group, "beatsync")), 1);
    connect(m_pButtonSync, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlBeatSync(double)),
            Qt::DirectConnection);

    m_pButtonSyncPhase = pControlManager->addControl(
        new ControlPushButton(ConfigKey(_group, "beatsync_phase")), 1);
    connect(m_pButtonSyncPhase, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlBeatSyncPhase(double)),
            Qt::DirectConnection);

    m_pButtonSyncTempo = pControlManager->addControl(
        new ControlPushButton(ConfigKey(_group, "beatsync_tempo")), 1);
    connect(m_pButtonSyncTempo, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlBeatSyncTempo(double)),
            Qt::DirectConnection);

    m_pTranslateBeats = pControlManager->addControl(
        new ControlPushButton(ConfigKey(_group, "beats_translate_curpos")), 1);
    connect(m_pTranslateBeats, SIGNAL(valueChanged(double)),
            this, SLOT(slotBeatsTranslate(double)),
            Qt::DirectConnection);

    connect(&m_tapFilter, SIGNAL(tapped(double,int)),
            this, SLOT(slotTapFilter(double,int)),
            Qt::DirectConnection);
}

BpmControl::~BpmControl() {
    delete m_pEngineBpm;
    delete m_pFileBpm;
    delete m_pButtonSync;
    delete m_pButtonSyncTempo;
    delete m_pButtonSyncPhase;
    delete m_pButtonTap;
    delete m_pTranslateBeats;
    delete m_pTrackWatcher;
}

double BpmControl::getBpm() {
    return m_pEngineBpm->get();
}

void BpmControl::slotFileBpmChanged(double bpm) {
    //qDebug() << this << "slotFileBpmChanged" << bpm;
    // Adjust the file-bpm with the current setting of the rate to get the
    // engine BPM.
    double dRate = 1.0 + m_pRateDir->get() * m_pRateRange->get() * m_pRateSlider->get();
    m_pEngineBpm->set(bpm * dRate);
}

void BpmControl::slotSetEngineBpm(double bpm) {
    double filebpm = m_pFileBpm->get();

    if (filebpm != 0.0) {
        double newRate = bpm / filebpm - 1.0f;
        newRate = math_max(-1.0f, math_min(1.0f, newRate));
        m_pRateSlider->set(newRate * m_pRateDir->get());
    }
}

void BpmControl::slotBpmTap(double v) {
    if (v > 0) {
        m_tapFilter.tap();
    }
}

void BpmControl::slotTapFilter(double averageLength, int numSamples) {
    // averageLength is the average interval in milliseconds tapped over
    // numSamples samples.  Have to convert to BPM now:

    if (averageLength <= 0)
        return;

    if (numSamples < 4)
        return;

    // (60 seconds per minute) * (1000 milliseconds per second) / (X millis per
    // beat) = Y beats/minute
    double averageBpm = 60.0 * 1000.0 / averageLength;
    m_pFileBpm->set(averageBpm);
    slotFileBpmChanged(averageBpm);
}

void BpmControl::slotControlBeatSyncPhase(double v) {
    if (!v)
        return;
    syncPhase();
}

void BpmControl::slotControlBeatSyncTempo(double v) {
    if (!v)
        return;
    syncTempo();
}

void BpmControl::slotControlBeatSync(double v) {
    if (!v)
        return;

    // If the player is playing, and adjusting its tempo succeeded, adjust its
    // phase so that it plays in sync.
    if (syncTempo() && m_pPlayButton->get() > 0) {
        syncPhase();
    }
}

bool BpmControl::syncTempo() {
    EngineBuffer* pOtherEngineBuffer = getOtherEngineBuffer();

    if(!pOtherEngineBuffer)
        return false;

    double fThisBpm  = m_pEngineBpm->get();
    //double fThisRate = m_pRateDir->get() * m_pRateSlider->get() * m_pRateRange->get();
    double fThisFileBpm = m_pFileBpm->get();

    double fOtherBpm = pOtherEngineBuffer->getBpm();
    double fOtherRate = pOtherEngineBuffer->getRate();
    double fOtherFileBpm = fOtherBpm / (1.0 + fOtherRate);

    //qDebug() << "this" << "bpm" << fThisBpm << "filebpm" << fThisFileBpm << "rate" << fThisRate;
    //qDebug() << "other" << "bpm" << fOtherBpm << "filebpm" << fOtherFileBpm << "rate" << fOtherRate;

    ////////////////////////////////////////////////////////////////////////////
    // Rough proof of how syncing works -- rryan 3/2011
    // ------------------------------------------------
    //
    // Let this and other denote this deck versus the sync-target deck.
    //
    // The goal is for this deck's effective BPM to equal the other decks.
    //
    // thisBpm = otherBpm
    //
    // The overall rate is the product of range, direction, and scale plus 1:
    //
    // rate = 1.0 + rateDir * rateRange * rateScale
    //
    // An effective BPM is the file-bpm times the rate:
    //
    // bpm = fileBpm * rate
    //
    // So our goal is to tweak thisRate such that this equation is true:
    //
    // thisFileBpm * (1.0 + thisRate) = otherFileBpm * (1.0 + otherRate)
    //
    // so rearrange this equation in terms of thisRate:
    //
    // thisRate = (otherFileBpm * (1.0 + otherRate)) / thisFileBpm - 1.0
    //
    // So the new rateScale to set is:
    //
    // thisRateScale = ((otherFileBpm * (1.0 + otherRate)) / thisFileBpm - 1.0) / (thisRateDir * thisRateRange)

    if (fOtherBpm > 0.0 && fThisBpm > 0.0) {
        // The desired rate is the other decks effective rate divided by this
        // deck's file BPM. This gives us the playback rate that will produe an
        // effective BPM equivalent to the other decks.
        double fDesiredRate = fOtherBpm / fThisFileBpm;

        // Test if this buffer's bpm is the double of the other one, and adjust
        // the rate scale. I believe this is intended to account for our BPM
        // algorithm sometimes finding double or half BPMs. This avoids drastic
        // scales.

        float fFileBpmDelta = fabs(fThisFileBpm-fOtherFileBpm);
        if (fabs(fThisFileBpm*2.0 - fOtherFileBpm) < fFileBpmDelta) {
            fDesiredRate /= 2.0;
        } else if (fabs(fThisFileBpm - 2.0*fOtherFileBpm) < fFileBpmDelta) {
            fDesiredRate *= 2.0;
        }

        // Subtract the base 1.0, now fDesiredRate is the percentage
        // increase/decrease in playback rate, not the playback rate.
        fDesiredRate -= 1.0;

        // Ensure the rate is within resonable boundaries. Remember, this is the
        // percent to scale the rate, not the rate itself. If fDesiredRate was -1,
        // that would mean the deck would be completely stopped. If fDesiredRate
        // is 1, that means it is playing at 2x speed. This limit enforces that
        // we are scaled between 0.5x and 2x.
        if (fDesiredRate < 1.0 && fDesiredRate > -0.5)
        {
            // Adjust the rateScale. We have to divide by the range and
            // direction to get the correct rateScale.
            fDesiredRate = fDesiredRate/(m_pRateRange->get() * m_pRateDir->get());

            // And finally, set the slider
            m_pRateSlider->set(fDesiredRate);
            return true;
        }
    }
    return false;
}

bool BpmControl::syncPhase() {
    EngineBuffer* pOtherEngineBuffer = getOtherEngineBuffer();
    TrackPointer otherTrack = pOtherEngineBuffer->getLoadedTrack();
    BeatsPointer otherBeats = otherTrack ? otherTrack->getBeats() : BeatsPointer();

    // If either track does not have beats, then we can't adjust the phase.
    if (!m_pBeats || !otherBeats) {
        return false;
    }

    // Get the file BPM of each song.
    //double dThisBpm = m_pBeats->getBpm();
    //double dOtherBpm = ControlObject::getControl(
    //ConfigKey(pOtherEngineBuffer->getGroup(), "file_bpm"))->get();

    // Get the current position of both decks
    double dThisPosition = getCurrentSample();
    double dOtherLength = ControlObject::getControl(
        ConfigKey(pOtherEngineBuffer->getGroup(), "track_samples"))->get();
    double dOtherPosition = dOtherLength * ControlObject::getControl(
        ConfigKey(pOtherEngineBuffer->getGroup(), "visual_playposition"))->get();

    double dThisPrevBeat = m_pBeats->findPrevBeat(dThisPosition);
    double dThisNextBeat = m_pBeats->findNextBeat(dThisPosition);

    if (dThisPrevBeat == -1 || dThisNextBeat == -1) {
        return false;
    }

    // Protect against the case where we are sitting exactly on the beat.
    if (dThisPrevBeat == dThisNextBeat) {
        dThisNextBeat = m_pBeats->findNthBeat(dThisPosition, 2);
    }

    double dOtherPrevBeat = otherBeats->findPrevBeat(dOtherPosition);
    double dOtherNextBeat = otherBeats->findNextBeat(dOtherPosition);

    if (dOtherPrevBeat == -1 || dOtherNextBeat == -1) {
        return false;
    }

    // Protect against the case where we are sitting exactly on the beat.
    if (dOtherPrevBeat == dOtherNextBeat) {
        dOtherNextBeat = otherBeats->findNthBeat(dOtherPosition, 2);
    }

    double dThisBeatLength = fabs(dThisNextBeat - dThisPrevBeat);
    double dOtherBeatLength = fabs(dOtherNextBeat - dOtherPrevBeat);
    double dOtherBeatFraction = (dOtherPosition - dOtherPrevBeat) / dOtherBeatLength;

    double dNewPlaypos;
    bool this_near_next = dThisNextBeat - dThisPosition <= dThisPosition - dThisPrevBeat;
    bool other_near_next = dOtherNextBeat - dOtherPosition <= dOtherPosition - dOtherPrevBeat;

    // We want our beat fraction to be identical to theirs.

    // If the two tracks have similar alignment, adjust phase is straight-
    // forward.  Use the same fraction for both beats, starting from the previous
    // beat.  But if This track is nearer to the next beat and the Other track
    // is nearer to the previous beat, use This Next beat as the starting point
    // for the phase. (ie, we pushed the sync button late).  If This track
    // is nearer to the previous beat, but the Other track is nearer to the
    // next beat, we pushed the sync button early so use the double-previous
    // beat as the basis for the adjustment.
    //
    // This makes way more sense when you're actually mixing.
    //
    // TODO(XXX) Revisit this logic once we move away from tempo-locked,
    // infinite beatgrids because the assumption that findNthBeat(-2) always
    // works will be wrong then.

    if (this_near_next == other_near_next) {
        dNewPlaypos = dThisPrevBeat + dOtherBeatFraction * dThisBeatLength;
    } else if (this_near_next && !other_near_next) {
        dNewPlaypos = dThisNextBeat + dOtherBeatFraction * dThisBeatLength;
    } else {  //!this_near_next && other_near_next
        dThisPrevBeat = m_pBeats->findNthBeat(dThisPosition, -2);
        dNewPlaypos = dThisPrevBeat + dOtherBeatFraction * dThisBeatLength;
    }

    emit(seekAbs(dNewPlaypos));
    return true;
}

void BpmControl::slotRateChanged(double) {
    double dFileBpm = m_pFileBpm->get();
    slotFileBpmChanged(dFileBpm);
}

void BpmControl::trackLoaded(TrackPointer pTrack) {
    if (m_pTrack) {
        trackUnloaded(m_pTrack);
    }

    if (pTrack) {
        m_pTrack = pTrack;
        m_pBeats = m_pTrack->getBeats();
        m_pTrackWatcher->watchTrack(m_pTrack);
    }
}

void BpmControl::trackUnloaded(TrackPointer pTrack) {
    if (m_pTrack) {
        m_pTrackWatcher->unwatchTrack(m_pTrack);
    }
    m_pTrack.clear();
    m_pBeats.clear();
}

void BpmControl::slotUpdatedTrackBeats() {
    //qDebug() << "BpmControl::slotUpdatedTrackBeats()";
    if (m_pTrack) {
        m_pBeats = m_pTrack->getBeats();
    }
}

void BpmControl::slotBeatsTranslate(double v) {
    if (v > 0 && m_pBeats && (m_pBeats->getCapabilities() & Beats::BEATSCAP_TRANSLATE)) {
        double currentSample = getCurrentSample();
        double closestBeat = m_pBeats->findClosestBeat(currentSample);
        int delta = currentSample - closestBeat;
        if (delta % 2 != 0) {
            delta--;
        }
        m_pBeats->translate(delta);
    }
}
