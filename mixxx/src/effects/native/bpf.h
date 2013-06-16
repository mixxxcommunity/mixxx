#ifndef BPF_H
#define BPF_H

#include <QMap>

#include "util.h"
#include "effects/effect.h"
#include "effects/native/nativebackend.h"

struct BpfState {
    CSAMPLE hzr1, hzl1;
    CSAMPLE hzr2, hzl2;
	CSAMPLE hpr1, hpl1;
	CSAMPLE hpr2, hpl2;
};

class Bpf : public Effect {
    Q_OBJECT
  public:
    Bpf (EffectsBackend* pBackend, EffectManifestPointer pManifest);
    virtual ~Bpf();

    static QString getId();
    static EffectManifestPointer getEffectManifest();
    static EffectPointer create(EffectsBackend* pBackend, EffectManifestPointer pManifest);

    // See effect.h
    void process(const QString channelId,
                 const CSAMPLE* pInput, CSAMPLE* pOutput,
                 const unsigned int numSamples);

  private:
    QString debugString() const {
        return "BPF";
    }
    BpfState* getStateForChannel(const QString channelId);

    EffectParameterPointer m_centerFreqParameter; // will be ratio: (frequency)/(sampling frequency)
    
    QMap<QString, BpfState*> m_bpfStates;

    DISALLOW_COPY_AND_ASSIGN(Bpf);
};


#endif /* BPF_H */
