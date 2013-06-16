#include <QtDebug>

#include "effects/native/bpf.h"

#include "mathstuff.h"
#include "sampleutil.h"

// static
QString Bpf::getId() {
    return "org.mixxx.effects.bpf";
}

// static
EffectManifestPointer Bpf::getEffectManifest() {
    EffectManifest* manifest = new EffectManifest();
    manifest->setId(getId());
    manifest->setName(QObject::tr("BPF"));
    manifest->setAuthor("The Mixxx Team");
    manifest->setVersion("1.0");
    manifest->setDescription("TODO");

    // center freq in Hz
    EffectManifestParameter* centerFreq = manifest->addParameter();
    centerFreq->setId("centerFreq");
    centerFreq->setName(QObject::tr("centerFreq"));
    centerFreq->setDescription("TODO");
    centerFreq->setControlHint(EffectManifestParameter::CONTROL_KNOB_LINEAR);
    centerFreq->setValueHint(EffectManifestParameter::VALUE_FLOAT);
    centerFreq->setSemanticHint(EffectManifestParameter::SEMANTIC_UNKNOWN);
    centerFreq->setUnitsHint(EffectManifestParameter::UNITS_UNKNOWN);
    centerFreq->setDefault(1010.0f);
    centerFreq->setMinimum(20.0f);
    centerFreq->setMaximum(20000.0f);

    // We don't use EffectManifestPointer here because that specifies const
    // EffectManifestParameter as the type, which does not work with
    // QObject::deleteLater.
    return QSharedPointer<EffectManifest>(manifest);
}

// static
EffectPointer Bpf::create(EffectsBackend* pBackend, EffectManifestPointer pManifest) {
    return EffectPointer(new Bpf(pBackend, pManifest));
}

Bpf::Bpf(EffectsBackend* pBackend, EffectManifestPointer pManifest)
        : Effect(pBackend, pManifest) {
    m_centerFreqParameter = getParameterFromId("centerFreq");
}

Bpf::~Bpf() {
    qDebug() << debugString() << "Bpfdestroyed";

    QMutableMapIterator<QString, BpfState*> it(m_bpfStates);

    while (it.hasNext()) {
        it.next();
        BpfState* pState = it.value();
        it.remove();
        delete pState;
    }
}

void Bpf::process(const QString channelId,
                            const CSAMPLE* pInput, CSAMPLE* pOutput,
                            const unsigned int numSamples) {

	// TODO (shanxS) dont need to compute coeff in every run. Fix it!
	// compute filter coeff.
	CSAMPLE a0, a1, a2, b1, b2;
	CSAMPLE centerFreq = m_centerFreqParameter->getValue().toDouble();

	CSAMPLE w0 = 2 * 3.141592653589 * centerFreq / 44100;
	CSAMPLE tmp = log (2)/2 * 2 * w0 /  sin (w0); // using BW as 2 octaves
	CSAMPLE alpha = sin (w0) * sinh (tmp);
	CSAMPLE c = cos (w0);

	a0 = alpha / (1+alpha);                      
	a1 = 0;
	a2 = -alpha / (1+alpha);
	b1 = -2 * c / (1+alpha);            
	b2 = (1-alpha) / (1+alpha);

	// get the channel status
	BpfState* pState = getStateForChannel(channelId);
	CSAMPLE hzr1 = pState->hzr1;
	CSAMPLE	hzl1 = pState->hzl1;
	CSAMPLE hzr2 = pState->hzr2;
	CSAMPLE hzl2 = pState->hzl2;
	CSAMPLE hpr1 = pState->hpr1;
	CSAMPLE hpl1 = pState->hpl1;
	CSAMPLE hpr2 = pState->hpr2;
	CSAMPLE hpl2 = pState->hpl2;

	// get loudness level before filtering
	CSAMPLE origVolumeL = 0, origVolumeR = 0, newVolumeL = 0, newVolumeR = 0;;
	SampleUtil::sumAbsPerChannel(&origVolumeL, &origVolumeR, pInput, numSamples);

	// do processing
	CSAMPLE out = 0.0;
		for (int i=0; i<numSamples; i++)
			if (i%2 == 0)
			{
				out = a0*pInput[i] + a1*hzl1 + a2*hzl2 - b1*hpl1 - b2*hpl2;
				hzl2 = hzl1;
				hzl1 = pInput[i];
				hpl2 = hpl1;
				hpl1 = out;
				pOutput[i] = out;
			}	
			else
			{		
				out = a0*pInput[i] + a1*hzr1 + a2*hzr2 - b1*hpr1 - b2*hpr2;
				hzr2 = hzr1;
				hzr1 = pInput[i];
				hpr2 = hpr1;
				hpr1 = out;
				pOutput[i] = out;
			}
	
	SampleUtil::sumAbsPerChannel(&newVolumeL, &newVolumeR, pInput, numSamples);
//	if (newVolumeR != 0 && newVolumeL !=0 )
	{
		SampleUtil::applyAlternatingGain(pOutput, origVolumeL/newVolumeL, origVolumeR/newVolumeR, numSamples);
		//qDebug() << "EffectoLRnLR: " << origVolumeL << " " << origVolumeR << " " << newVolumeL << " " << newVolumeR;
	}

	// store the channel status
	pState->hzr1 = hzr1;
	pState->hzl1 = hzl1;
	pState->hzr2 = hzr2;
	pState->hzl2 = hzl2;
	pState->hpr1 = hpr1;
	pState->hpl1 = hpl1;
	pState->hpr2 = hpr2;
	pState->hpl2 = hpl2;

	qDebug() << "bpf freq " << centerFreq;
}

BpfState* Bpf::getStateForChannel(const QString channelId) {
    BpfState* pState = NULL;
    if (!m_bpfStates.contains(channelId)) {
        pState = new BpfState();
        /*m_parametricEqStates[channelId] = pState;
        SampleUtil::applyGain(pState->delayBuffer, 0.0f, kMaxDelay);
        pState->delayPos = 0;
        pState->time = 0;
	*/
    } else {
        pState = m_bpfStates[channelId];
    }
    return pState;
}
