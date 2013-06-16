#include <QMutexLocker>
#include <QtDebug>

#include "effects/effectslot.h"

// The maximum number of effect parameters we're going to support.
const unsigned int kMaxParameters = 20;

EffectSlot::EffectSlot(QObject* pParent, const unsigned int iChainNumber, const unsigned int iSlotNumber)
        : QObject(),
          m_mutex(QMutex::Recursive),
          m_iChainNumber(iChainNumber),
          m_iSlotNumber(iSlotNumber),
          // The control group names are 1-indexed while internally everything is 0-indexed.
          m_group(formatGroupString(m_iChainNumber, m_iSlotNumber)) {
    m_pControlEnabled = new ControlObject(ConfigKey(m_group, "enabled"));
    m_pControlNumParameters = new ControlObject(ConfigKey(m_group, "num_parameters"));

    m_pControlDeleteEffect = new ControlObject(ConfigKey(m_group, "delete"));
    connect (m_pControlDeleteEffect, SIGNAL (valueChanged(double)),
             this, SLOT (slotDeleteEffect (double)));

    m_pControlMoveEffect = new ControlObject(ConfigKey(m_group, "move_effect"));
    connect (m_pControlMoveEffect, SIGNAL (valueChangedFromEngine(double)),
             this, SLOT (slotMoveEffect (double)));

    m_pControlMoveEffectUpOne = new ControlObject(ConfigKey(m_group, "move_effect_up_one"));
    connect (m_pControlMoveEffectUpOne , SIGNAL (valueChanged(double)),
             this, SLOT (slotMoveEffectUpOne (double)));

    m_pControlMoveEffectDownOne = new ControlObject(ConfigKey(m_group, "move_effect_down_one"));
    connect (m_pControlMoveEffectDownOne , SIGNAL (valueChanged(double)),
             this, SLOT (slotMoveEffectDownOne (double)));

    for (unsigned int i = 0; i < kMaxParameters; ++i) {
        EffectParameterSlot* pParameter = new EffectParameterSlot(this, m_iChainNumber,
                                                                  m_iSlotNumber, m_parameters.size());
        m_parameters.append(pParameter);
    }

    clear();
}

EffectSlot::~EffectSlot() {
    qDebug() << debugString() << "destroyed";
    clear();

    delete m_pControlEnabled;
    delete m_pControlNumParameters;
    delete m_pControlDeleteEffect;
    delete m_pControlMoveEffect; 
    delete m_pControlMoveEffectUpOne; 
    delete m_pControlMoveEffectDownOne; 

    while (!m_parameters.isEmpty()) {
        EffectParameterSlot* pParameter = m_parameters.takeLast();
        delete pParameter;
    }
}

QString EffectSlot::name()
{
    QMutexLocker locker(&m_mutex);
    if (m_pEffect)
        return m_pEffect->getManifest()->name();
    return tr("None");
}

int EffectSlot::numSlots()
{
    QMutexLocker locker(&m_mutex);
    if (m_pEffect)
        return m_pEffect->getManifest()->parameters().size();
    return 0;
}

EffectPointer EffectSlot::getEffect() const {
    QMutexLocker locker(&m_mutex);
    return m_pEffect;
}

void EffectSlot::loadEffect(EffectPointer pEffect) {
    qDebug() << debugString() << "loadEffect" << (pEffect ? pEffect->getManifest()->name() : "(null)");
    QMutexLocker locker(&m_mutex);
    if (pEffect) {
        m_pEffect = pEffect;
        m_pControlEnabled->set(1.0f);
        m_pControlNumParameters->set(m_pEffect->getManifest()->parameters().size());

        foreach (EffectParameterSlot* pParameter, m_parameters) {
            pParameter->loadEffect(m_pEffect);
        }

        // Always unlock before signalling to prevent deadlock
        locker.unlock();
        emit(effectLoaded(m_pEffect, m_iSlotNumber));
    } else {
        clear();
        locker.unlock();
        // Broadcasts a null effect pointer
        emit(effectLoaded(m_pEffect, m_iSlotNumber));
    }
}

void EffectSlot::slotDeleteEffect (double v)
{
    qDebug() << debugString() << "slotDeleteEffect" << (m_pEffect ? m_pEffect->getManifest()->name() : "(null)");
    QMutexLocker locker(&m_mutex);
    if (v > 0) 
        emit (deleteEffect (m_pEffect, m_iSlotNumber));

}

void EffectSlot::slotMoveEffect (double v)
{
    qDebug() << debugString() << "slotMoveEffect" << (m_pEffect ? m_pEffect->getManifest()->name() : "(null)");
    QMutexLocker locker(&m_mutex);
    if (v != 0)
    {
        emit (moveEffect (m_pEffect, m_iSlotNumber, static_cast<int> (v)));
        m_pControlMoveEffect->set(0.0f);
    }

}

void EffectSlot::slotMoveEffectUpOne (double v)
{
    qDebug() << debugString() << "slotMoveEffectUpOne" << (m_pEffect ? m_pEffect->getManifest()->name() : "(null)");
    QMutexLocker locker(&m_mutex);
    if (v != 0) 
        m_pControlMoveEffect->set(-1.0f);

}

void EffectSlot::slotMoveEffectDownOne (double v)
{
    qDebug() << debugString() << "slotMoveEffectDownOne" << (m_pEffect ? m_pEffect->getManifest()->name() : "(null)");
    QMutexLocker locker(&m_mutex);
    if (v != 0) 
        m_pControlMoveEffect->set(1.0f);

}

void EffectSlot::clear() {
    m_pEffect.clear();
    m_pControlEnabled->set(0.0f);
    m_pControlNumParameters->set(0.0f);
    m_pControlDeleteEffect->set(0.0f);
    m_pControlMoveEffect->set(0.0f);
    m_pControlMoveEffectUpOne->set(0.0f);
    m_pControlMoveEffectDownOne->set(0.0f);
    foreach (EffectParameterSlot* pParameter, m_parameters) {
        pParameter->loadEffect(EffectPointer());
    }
}
