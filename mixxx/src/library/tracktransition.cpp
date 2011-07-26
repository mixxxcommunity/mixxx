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
}

void TrackTransition::transition(QString groupA, QString groupB, TrackPointer trackA) {

    // Crossfading from Player 1 to Player 2
    if (groupA == "[Channel1]" && groupB == "[Channel2]") {

        float sampleThreshold = trackA->getFadeOut() / m_pCOTrackSamples1->get();
        float fadeLengthRatio = (float) m_iFadeLength / trackA->getDuration();
        float posFadeEnd = sampleThreshold + fadeLengthRatio;

        float crossfadeValue = -1.0f + 2*(m_pCOPlayPos1->get()-sampleThreshold) / (posFadeEnd - sampleThreshold);
        m_pCOCrossfader->slotSet(crossfadeValue);
    }

    // Crossfading from Player2 to Player 1
    if (groupA == "[Channel2]" && groupB == "[Channel1]") {

        float sampleThreshhold = trackA->getFadeOut() / m_pCOTrackSamples2->get();
        float fadeLengthRatio = (float) m_iFadeLength / trackA->getDuration();
        float posFadeEnd = sampleThreshhold + fadeLengthRatio;

        float crossfadeValue = -1.0f + 2*(m_pCOPlayPos2->get()-sampleThreshhold) / (posFadeEnd - sampleThreshhold);
        m_pCOCrossfader->slotSet(crossfadeValue);

    }
}
