#ifndef EFFECTSLOT_H
#define EFFECTSLOT_H

#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>

#include "util.h"
#include "controlobject.h"
#include "effects/effect.h"
#include "effects/effectparameterslot.h"

class EffectSlot;
typedef QSharedPointer<EffectSlot> EffectSlotPointer;

class EffectSlot : public QObject {
    Q_OBJECT
  public:
    EffectSlot(QObject* pParent, const unsigned int iChainNumber, const unsigned int iSlotNumber);
    virtual ~EffectSlot();

    static QString formatGroupString(const unsigned int iChainNumber, const unsigned int iSlotNumber) {
        return QString("[EffectChain%1_Effect%2]").arg(iChainNumber+1).arg(iSlotNumber+1);
    }

    // Get human readable name for currently loaded effect
    QString name();

    // Get number of active slots for loaded effect
    int numSlots();

    // Return the currently loaded effect, if any. If no effect is loaded,
    // returns a null EffectPointer.
    EffectPointer getEffect() const;

  signals:
    // Indicates that the effect pEffect has been loaded into this
    // EffectSlot. The slotNumber is provided for the convenience of listeners.
    // pEffect may be an invalid pointer, which indicates that a previously
    // loaded effect was removed from the slot.
    void effectLoaded(EffectPointer pEffect, unsigned int slotNumber);

    // Indicates that the effect pEffect, loaded into this
    // EffectSlot, has request its deletion. The slotNumber is provided for
    // convenience of listeners.
    void deleteEffect(EffectPointer pEffect, unsigned int slotNumber);

    // Indicates that the effect pEffect, loaded into this
    // EffectSlot, wants to move by deltaPos positions in EffectChain
    // The slotNumber is provided for convenience of listeners.
    void moveEffect(EffectPointer pEffect, unsigned int slotNumber, int deltaPos);

  public slots:
    // Request that this EffectSlot load the given Effect
    void loadEffect(EffectPointer pEffect);

  private slots:
    void slotDeleteEffect (double);
    void slotMoveEffect (double);
    void slotMoveEffectUpOne (double);
    void slotMoveEffectDownOne (double);

  private:
    QString debugString() const {
        return QString("EffectSlot(%1,%2)").arg(m_iChainNumber).arg(m_iSlotNumber);
    }

    // Unload the currently loaded effect
    void clear();

    mutable QMutex m_mutex;
    const unsigned int m_iChainNumber;
    const unsigned int m_iSlotNumber;
    const QString m_group;
    EffectPointer m_pEffect;

    ControlObject* m_pControlEnabled;
    ControlObject* m_pControlNumParameters;
    ControlObject* m_pControlDeleteEffect;
    ControlObject* m_pControlMoveEffect;
    ControlObject* m_pControlMoveEffectUpOne;
    ControlObject* m_pControlMoveEffectDownOne;
    QList<EffectParameterSlot*> m_parameters;

    DISALLOW_COPY_AND_ASSIGN(EffectSlot);
};

#endif /* EFFECTSLOT_H */
