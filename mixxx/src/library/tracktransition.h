#ifndef TRACKTRANSITION_H
#define TRACKTRANSITION_H

#include <QObject>

#include "configobject.h"

class ControlObjectThreadMain;

class TrackTransition : public QObject {
    Q_OBJECT

public:
    TrackTransition(QObject* parent, ConfigObject<ConfigValue>* pConfig);
    virtual ~TrackTransition();

private:
    // This is the transition function used by AutoDJ to crossfade from the desired
    // deckA to deckB
    void transition(QString groupA, QString groupB);

    // The user defined crossfade length
    int m_iFadeLength;

    ConfigObject<ConfigValue>* m_pConfig;
    ControlObjectThreadMain* m_pCOCrossfader;

    friend class AutoDJ;

};

#endif // TRACKTRANSITION_H
