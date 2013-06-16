#ifndef WEFFECTCHAIN_H
#define WEFFECTCHAIN_H

#include <QWidget>
#include <QLabel>

#include "effects/effectchainslot.h"
#include "effects/effectsmanager.h"

class WEffectChain : public QLabel {
    Q_OBJECT

  public:
    WEffectChain(unsigned int chainNumber, QWidget* pParent=NULL);
    virtual ~WEffectChain();

  private slots:
    void chainUpdated();

  private:
    // Set the EffectChain that should be monitored by this WEffectChain
    void setEffectChainSlot(EffectChainSlotPointer pEffectChainSlot);

    EffectChainSlotPointer m_pEffectChainSlot;
};

#endif /* WEFFECTCHAIN_H */
