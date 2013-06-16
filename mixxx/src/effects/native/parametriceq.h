#ifndef PARAMETRICEQ_H
#define PARAMETRICEQ_H

#include <QMap>

#include "util.h"
#include "effects/effect.h"
#include "effects/native/nativebackend.h"

struct ParametricEqState {
    CSAMPLE hzr1, hzl1;
    CSAMPLE hzr2, hzl2;
	CSAMPLE hpr1, hpl1;
	CSAMPLE hpr2, hpl2;
};

class ParametricEq : public Effect {
    Q_OBJECT
  public:
    ParametricEq (EffectsBackend* pBackend, EffectManifestPointer pManifest);
    virtual ~ParametricEq();

    static QString getId();
    static EffectManifestPointer getEffectManifest();
    static EffectPointer create(EffectsBackend* pBackend, EffectManifestPointer pManifest);

    // See effect.h
    void process(const QString channelId,
                 const CSAMPLE* pInput, CSAMPLE* pOutput,
                 const unsigned int numSamples);

  private:
    QString debugString() const {
        return "ParametricEq";
    }
    ParametricEqState* getStateForChannel(const QString channelId);

    EffectParameterPointer m_centerFreqParameter; // will be ratio: (frequency)/(sampling frequency)
    EffectParameterPointer m_bandwidthParameter; // in octaves
    EffectParameterPointer m_boostParameter; // boost or attenuate

    QMap<QString, ParametricEqState*> m_parametricEqStates;

    DISALLOW_COPY_AND_ASSIGN(ParametricEq);
};


#endif /* PARAMETRICEQ_H */
