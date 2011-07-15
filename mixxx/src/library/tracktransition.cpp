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
}

TrackTransition::~TrackTransition() {
    delete m_pCOCrossfader;
}

void TrackTransition::transition(QString groupA, QString groupB) {

    if (groupA == "[Channel1]" && groupB == "[Channel2]") {
        // Crossfading from Player 1 to Player 2

    }

    if (groupA == "[Channel2]" && groupB == "[Channel1]") {
        // Crossfading from Player 2 to Player 1

    }
}
