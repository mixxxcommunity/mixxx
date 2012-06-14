/*
 * tracktransition.cpp
 *
 *  Created on: Jun 7, 2012
 *      Author: scottstewart
 */

#include "controlobject.h"
#include "controlobjectthreadmain.h"
#include "tracktransition.h"

TrackTransition::TrackTransition(QObject* parent, ConfigObject<ConfigValue>* pConfig) :
    QObject(parent),
    m_pConfig(pConfig) {

    // TODO(tom__m) This needs to be updated when the preferences are changed.
    m_iFadeLength = m_pConfig->getValueString(
                            ConfigKey("[Auto DJ]", "CrossfadeLength")).toInt();

    m_pCOCrossfader = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Master]", "crossfader")));
    m_pCOPlayPos1 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel1]", "playposition")));
    m_pCOPlayPos2 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel2]", "playposition")));
    m_pCOTrackSamples1 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel1]", "track_samples")));
    m_pCOTrackSamples2 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel2]", "track_samples")));
}

TrackTransition::~TrackTransition() {
    delete m_pCOCrossfader;
    delete m_pCOPlayPos1;
    delete m_pCOPlayPos2;
    delete m_pCOTrackSamples1;
    delete m_pCOTrackSamples2;
}

void TrackTransition::transition(QString groupA, QString groupB, TrackPointer trackA) {

    // Crossfading from Player 1 to Player 2
    if (groupA == "[Channel1]" && groupB == "[Channel2]") {

        // Find the FadeOut point position and crossfade length as ratios
        // of the whole track. posFadeEnd is then the position where the crossfade
        // will stop.
        float sampleThreshold = trackA->getFadeOut() / m_pCOTrackSamples1->get();
        float fadeLengthRatio = (float) m_iFadeLength / trackA->getDuration();
        float posFadeEnd = sampleThreshold + fadeLengthRatio;

        // Check if our crossfade would stop past the end of the track
        if (posFadeEnd > 1.0) {
            posFadeEnd = 1.0;
        }

        float crossfadeValue = -1.0f + 2*(m_pCOPlayPos1->get()-sampleThreshold) / (posFadeEnd - sampleThreshold);
        m_pCOCrossfader->slotSet(crossfadeValue);
    }

    // Crossfading from Player2 to Player 1
    if (groupA == "[Channel2]" && groupB == "[Channel1]") {

        // Find the FadeOut point position and crossfade length as ratios
        // of the whole track. posFadeEnd is then the position where the crossfade
        // will stop.
        float sampleThreshhold = trackA->getFadeOut() / m_pCOTrackSamples2->get();
        float fadeLengthRatio = (float) m_iFadeLength / trackA->getDuration();
        float posFadeEnd = sampleThreshhold + fadeLengthRatio;

        // Check if our crossfade would stop past the end of the track
        if (posFadeEnd > 1.0) {
            posFadeEnd = 1.0;
        }

        float crossfadeValue = 1.0f - 2*(m_pCOPlayPos2->get()-sampleThreshhold) / (posFadeEnd - sampleThreshhold);
        m_pCOCrossfader->slotSet(crossfadeValue);

    }
}
