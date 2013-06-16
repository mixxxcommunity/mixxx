#include <QtDebug>

#include "effects/effectsmanager.h"

#include "effects/effectchainmanager.h"

// TODO(rryan) REMOVE
#include "effects/native/flangereffect.h"
#include "effects/native/bpf.h"
#include "effects/native/parametriceq.h"

EffectsManager* EffectsManager::m_thisPointer = NULL;
EffectsManager* EffectsManager::getEffectsManager ()
{
    if (!m_thisPointer)
        qWarning() << "EffectsManager required when it doenst exist";

    return m_thisPointer;
}

EffectsManager::EffectsManager(QObject* pParent)
        : QObject(pParent),
          m_mutex(QMutex::Recursive),
          iNewChainNumber (0) {
    m_pEffectChainManager = new EffectChainManager(this);
    m_thisPointer = this;
}

EffectsManager::~EffectsManager() {
    m_effectChainSlots.clear();
    while (!m_effectsBackends.isEmpty()) {
        EffectsBackend* pBackend = m_effectsBackends.takeLast();
        delete pBackend;
    }
    delete m_pEffectChainManager;
    m_thisPointer = NULL;
}

void EffectsManager::addEffectsBackend(EffectsBackend* pBackend) {
    QMutexLocker locker(&m_mutex);
    Q_ASSERT(pBackend);
    m_effectsBackends.append(pBackend);
}

unsigned int EffectsManager::numEffectChainSlots() const {
    QMutexLocker locker(&m_mutex);
    return m_effectChainSlots.size();
}

void EffectsManager::addEffectChainSlot() {
    QMutexLocker locker(&m_mutex);
    EffectChainSlot* pChainSlot = new EffectChainSlot(this, m_effectChainSlots.size());

    // TODO(rryan) How many should we make default? They create controls that
    // the GUI may rely on, so the choice is important to communicate to skin
    // designers.
    pChainSlot->addEffectSlot();
    pChainSlot->addEffectSlot();
    pChainSlot->addEffectSlot();
    pChainSlot->addEffectSlot();

    connect(pChainSlot, SIGNAL(nextChain(const unsigned int, EffectChainPointer)),
            this, SLOT(loadNextChain(const unsigned int, EffectChainPointer)));
    connect(pChainSlot, SIGNAL(prevChain(const unsigned int, EffectChainPointer)),
            this, SLOT(loadPrevChain(const unsigned int, EffectChainPointer)));
    connect(pChainSlot, SIGNAL(addChain(const unsigned int)),
            this, SLOT(loadNewlyAddedChain(const unsigned int)));
    connect(pChainSlot, SIGNAL(deleteChain(const unsigned int)),
            this, SLOT(deleteAndLoadNextChain(const unsigned int)));

    // Register all the existing channels with the new EffectChain
    foreach (QString channelId, m_registeredChannels) {
        pChainSlot->registerChannel(channelId);
    }

    m_effectChainSlots.append(EffectChainSlotPointer(pChainSlot));
}

EffectChainSlotPointer EffectsManager::getEffectChainSlot(unsigned int i) {
    QMutexLocker locker(&m_mutex);
    if (i >= m_effectChainSlots.size()) {
        qDebug() << "WARNING: Invalid index for getEffectChainSlot";
        return EffectChainSlotPointer();
    }
    return m_effectChainSlots[i];
}

void EffectsManager::process(const QString channelId,
                             const CSAMPLE* pInput, CSAMPLE* pOutput,
                             const unsigned int numSamples) {
    QMutexLocker locker(&m_mutex);

    QList<EffectChainPointer> enabledEffects;

    for (int i = 0; i < m_effectChainSlots.size(); ++i) {
        EffectChainSlotPointer pChainSlot = m_effectChainSlots[i];
        if (pChainSlot->isEnabledForChannel(channelId))
            enabledEffects.append(pChainSlot->getEffectChain());
    }

    ////////////////////////////////////////////////////////////////////////////
    // AFTER THIS LINE, THE MUTEX IS UNLOCKED. DONT TOUCH ANY MEMBER STATE
    ////////////////////////////////////////////////////////////////////////////
    locker.unlock();

    bool inPlace = pInput == pOutput;
    for (int i = 0; i < enabledEffects.size(); ++i) {
        EffectChainPointer pChain = enabledEffects[i];

        if (!pChain)
            continue;

        if (inPlace) {
            // Since we're doing this in-place, using a temporary buffer doesn't
            // matter.
            pChain->process(channelId, pInput, pOutput, numSamples);
        } else {
            qDebug() << debugString() << "WARNING: non-inplace processing not implemented!";
            // TODO(rryan) implement. Trickier because you have to use temporary
            // memory. Punting this for now just to get everything working.
        }
    }
}

void EffectsManager::registerChannel(const QString channelId) {
    QMutexLocker locker(&m_mutex);
    if (m_registeredChannels.contains(channelId)) {
        qDebug() << debugString() << "WARNING: Channel already registered:" << channelId;
        return;
    }

    m_registeredChannels.insert(channelId);
    foreach (EffectChainSlotPointer pEffectChainSlot, m_effectChainSlots) {
        pEffectChainSlot->registerChannel(channelId);
    }
}

void EffectsManager::loadNextChain(const unsigned int iChainSlotNumber, EffectChainPointer pLoadedChain) {
    QMutexLocker locker(&m_mutex);
    EffectChainPointer pNextChain = m_pEffectChainManager->getNextEffectChain(pLoadedChain);
    m_effectChainSlots[iChainSlotNumber]->loadEffectChain(pNextChain);
}


void EffectsManager::loadPrevChain(const unsigned int iChainSlotNumber, EffectChainPointer pLoadedChain) {
    QMutexLocker locker(&m_mutex);
    EffectChainPointer pPrevChain = m_pEffectChainManager->getPrevEffectChain(pLoadedChain);
    m_effectChainSlots[iChainSlotNumber]->loadEffectChain(pPrevChain);
}

void EffectsManager::loadNewlyAddedChain(const unsigned int iChainSlotNumber) {
    QMutexLocker locker(&m_mutex);

    EffectChainPointer pChain = EffectChainPointer(new EffectChain());
    pChain->setId(newChainId());
    pChain->setName(newChainName());
    pChain->setParameter(0.0f);

    m_pEffectChainManager->addEffectChain(pChain);
    m_effectChainSlots[iChainSlotNumber]->loadEffectChain(pChain);
}

void EffectsManager::deleteAndLoadNextChain(const unsigned int iChainSlotNumber) {
    QMutexLocker locker(&m_mutex);

    EffectChainPointer pCurrentChain = m_effectChainSlots[iChainSlotNumber]->getEffectChain();
    EffectChainPointer pNextChain = m_pEffectChainManager->deleteAndGetNextEffectChain(pCurrentChain);
    m_effectChainSlots[iChainSlotNumber]->loadEffectChain(pNextChain);
}

const QSet<QString> EffectsManager::getAvailableEffects() const {
    QMutexLocker locker(&m_mutex);
    QSet<QString> availableEffects;

    foreach (EffectsBackend* pBackend, m_effectsBackends) {
        QSet<QString> backendEffects = pBackend->getEffectIds();
        foreach (QString effectId, backendEffects) {
            if (availableEffects.contains(effectId)) {
                qDebug() << "WARNING: Duplicate effect ID" << effectId;
                continue;
            }
            availableEffects.insert(effectId);
        }
    }

    return availableEffects;
}

EffectManifestPointer EffectsManager::getEffectManifest(const QString effectId) const {
    QMutexLocker locker(&m_mutex);

    foreach (EffectsBackend* pBackend, m_effectsBackends) {
        if (pBackend->canInstantiateEffect(effectId)) {
            return pBackend->getManifest(effectId);
        }
    }

    return EffectManifestPointer();
}

EffectPointer EffectsManager::instantiateEffect(const QString effectId) {
    QMutexLocker locker(&m_mutex);

    foreach (EffectsBackend* pBackend, m_effectsBackends) {
        if (pBackend->canInstantiateEffect(effectId)) {
            return pBackend->instantiateEffect(effectId);
        }
    }

    return EffectPointer();
}

void EffectsManager::setupDefaultChains() {
QMutexLocker locker(&m_mutex);
    QSet<QString> effects = getAvailableEffects();

    QString flangerId = FlangerEffect::getId();
    QString parametricEqId = ParametricEq::getId();
	QString bpfId = Bpf::getId();

    if (effects.contains(flangerId)) {
        EffectChainPointer pChain = EffectChainPointer(new EffectChain());
        pChain->setId("org.mixxx.effectchain.flanger");
        pChain->setName(tr("Flanger"));
        pChain->setParameter(0.0f);

        EffectPointer flanger = instantiateEffect(flangerId);
        pChain->addEffect(flanger);
        m_pEffectChainManager->addEffectChain(pChain);
	

        pChain = EffectChainPointer(new EffectChain());
        pChain->setId("org.mixxx.effectchain.parametricEq");
        pChain->setName(tr("ParametricEq"));
        pChain->setParameter(0.0f);

        EffectPointer parametricEq = instantiateEffect(parametricEqId);
        pChain->addEffect(parametricEq);
        m_pEffectChainManager->addEffectChain(pChain);

        pChain = EffectChainPointer(new EffectChain());
        pChain->setId("org.mixxx.effectchain.bpf");
        pChain->setName(tr("BPF"));
        pChain->setParameter(0.0f);

        EffectPointer bpf = instantiateEffect(bpfId);
        pChain->addEffect(bpf);
        m_pEffectChainManager->addEffectChain(pChain);
	
    }

}

