#include <QtDebug>

#include "effects/native/nativebackend.h"
#include "effects/native/flangereffect.h"
#include "effects/native/parametriceq.h"
#include "effects/native/bpf.h"

// void registerEffect(QString id,
//                     EffectManifestPointer (*manifest)(),
//                     EffectPointer (*create)(EffectsBackend*, EffectManifestPointer)) {
//     qDebug() << "register" << id;
// }

NativeBackend::NativeBackend(QObject* pParent)
        : EffectsBackend(pParent, tr("Native")) {
    EffectManifestPointer flanger = FlangerEffect::getEffectManifest();
    m_effectManifests.append(flanger);
    registerEffect(FlangerEffect::getId(), flanger, &FlangerEffect::create);

    EffectManifestPointer parametricEq = ParametricEq::getEffectManifest();
    m_effectManifests.append(parametricEq);
    registerEffect(ParametricEq::getId(), parametricEq, &ParametricEq::create);

	EffectManifestPointer bpf = Bpf::getEffectManifest();
    m_effectManifests.append(bpf);
    registerEffect(Bpf::getId(), bpf, &Bpf::create);


}

NativeBackend::~NativeBackend() {
    qDebug() << debugString() << "destroyed";
    m_effectManifests.clear();
}

// const QList<QString> NativeBackend::getAvailableEffects() const {
//     return m_effectManifests;
// }

// EffectPointer NativeBackend::instantiateEffect(EffectManifestPointer manifest) {
//     // TODO(rryan) effect instantiation sucks. should fix this before we commit
//     // to it being this way.
//     if (manifest->id() == "org.mixxx.effects.flanger") {
//         EffectPointer flanger = EffectPointer(new FlangerEffect(this, manifest));
//         return flanger;
//     }
//     return EffectPointer();
// }
