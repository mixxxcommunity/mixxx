#include "widget/weffectchain.h"

WEffectChain::WEffectChain(unsigned int chainNumber, QWidget* pParent)
        : QLabel(pParent),
          m_pEffectChainSlot () {

    EffectsManager *pManager = EffectsManager::getEffectsManager ();
    if (!pManager)
        qWarning() << "EffectsManager is not availabe - Cannot initiate Effectchain" + chainNumber;
    else
        setEffectChainSlot (pManager->getEffectChainSlot (chainNumber));

    // TODO (shanxS) this is ugly...
    // use config from skin.xml to i
    // set these limits
    setFixedSize (100, 20);
}

WEffectChain::~WEffectChain() {
}

void WEffectChain::setEffectChainSlot(EffectChainSlotPointer effectChainSlot) {
    if (effectChainSlot) {
        m_pEffectChainSlot = effectChainSlot;
        connect(effectChainSlot.data(), SIGNAL(chainRenamed(QString)),
                this, SLOT(setText(QString)));
        connect(effectChainSlot.data(), SIGNAL(updated()),
                this, SLOT(chainUpdated()));
        chainUpdated();
    }
}

void WEffectChain::chainUpdated() {
    if (m_pEffectChainSlot) {
        setText(m_pEffectChainSlot->name());
    }
}
