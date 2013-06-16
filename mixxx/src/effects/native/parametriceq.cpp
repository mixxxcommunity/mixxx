#include <QtDebug>

#include "effects/native/parametriceq.h"

#include "mathstuff.h"
#include "sampleutil.h"

// static
QString ParametricEq::getId() {
    return "org.mixxx.effects.parametriceq";
}

// static
EffectManifestPointer ParametricEq::getEffectManifest() {
    EffectManifest* manifest = new EffectManifest();
    manifest->setId(getId());
    manifest->setName(QObject::tr("ParametricEq"));
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

    // bandwidth in octaves
    EffectManifestParameter* bandwidth = manifest->addParameter();
    bandwidth->setId("bandwidth");
    bandwidth->setName(QObject::tr("Bandwidth"));
    bandwidth->setDescription("TODO");
    bandwidth->setControlHint(EffectManifestParameter::CONTROL_KNOB_LINEAR);
    bandwidth->setValueHint(EffectManifestParameter::VALUE_FLOAT);
    bandwidth->setSemanticHint(EffectManifestParameter::SEMANTIC_UNKNOWN);
    bandwidth->setUnitsHint(EffectManifestParameter::UNITS_UNKNOWN);
    bandwidth->setDefault(3.0f);
    bandwidth->setMinimum(2.0f);
    bandwidth->setMaximum(5.0f);

    // boost or cut in dB
    EffectManifestParameter* boost = manifest->addParameter();
    boost->setId("boost");
    boost->setName(QObject::tr("Boost"));
    boost->setDescription("TODO");
    boost->setControlHint(EffectManifestParameter::CONTROL_KNOB_LINEAR);
    boost->setValueHint(EffectManifestParameter::VALUE_FLOAT);
    boost->setSemanticHint(EffectManifestParameter::SEMANTIC_UNKNOWN);
    boost->setUnitsHint(EffectManifestParameter::UNITS_UNKNOWN);
    boost->setDefault(0.0f);
    boost->setMinimum(-20.0f);
    boost->setMaximum(20.0f);

    // We don't use EffectManifestPointer here because that specifies const
    // EffectManifestParameter as the type, which does not work with
    // QObject::deleteLater.
    return QSharedPointer<EffectManifest>(manifest);
}

// static
EffectPointer ParametricEq::create(EffectsBackend* pBackend, EffectManifestPointer pManifest) {
    return EffectPointer(new ParametricEq(pBackend, pManifest));
}

ParametricEq::ParametricEq(EffectsBackend* pBackend, EffectManifestPointer pManifest)
        : Effect(pBackend, pManifest) {
    m_centerFreqParameter = getParameterFromId("centerFreq");
    m_bandwidthParameter = getParameterFromId("bandwidth");
    m_boostParameter = getParameterFromId("boost");
}

ParametricEq::~ParametricEq() {
    qDebug() << debugString() << "ParametricEqdestroyed";

    QMutableMapIterator<QString, ParametricEqState*> it(m_parametricEqStates);

    while (it.hasNext()) {
        it.next();
        ParametricEqState* pState = it.value();
        it.remove();
        delete pState;
    }
}

void ParametricEq::process(const QString channelId,
                            const CSAMPLE* pInput, CSAMPLE* pOutput,
                            const unsigned int numSamples) {

	// TODO (shanxS) dont need to compute coeff in every run. Fix it!
	// compute filter coeff.
	CSAMPLE a0, a1, a2, b1, b2;
	CSAMPLE centerFreq = m_centerFreqParameter->getValue().toDouble();
	CSAMPLE boost = m_boostParameter->getValue().toDouble();
	CSAMPLE bandwidth = m_bandwidthParameter->getValue().toDouble();

	qDebug () << "PRAMETRICEQ " << centerFreq << " " << boost;

	CSAMPLE w0 = 2 * 3.141592653589 * centerFreq / 44100;
	CSAMPLE a = pow (10, (boost/40));
	CSAMPLE tmp = log (2)/2 * bandwidth * w0 /  sin (w0);
	CSAMPLE alpha = sin (w0) * sinh (tmp);
	CSAMPLE c = cos (w0);
	CSAMPLE s = sin (w0);

	a0 = (1 + alpha*a)/(1 + alpha/a);
	a1 = -2*c/(1 + alpha/a);
	a2 = (1 - alpha*a)/(1 + alpha/a);
	b1 = -2*c/(1 + alpha/a);
	b2 = (1 - alpha/a)/(1 + alpha/a);

	// get the channel status
	ParametricEqState* pState = getStateForChannel(channelId);
	CSAMPLE hzr1 = pState->hzr1;
	CSAMPLE	hzl1 = pState->hzl1;
	CSAMPLE hzr2 = pState->hzr2;
	CSAMPLE hzl2 = pState->hzl2;
	CSAMPLE hpr1 = pState->hpr1;
	CSAMPLE hpl1 = pState->hpl1;
	CSAMPLE hpr2 = pState->hpr2;
	CSAMPLE hpl2 = pState->hpl2;

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
	

	// store the channel status
	pState->hzr1 = hzr1;
	pState->hzl1 = hzl1;
	pState->hzr2 = hzr2;
	pState->hzl2 = hzl2;
	pState->hpr1 = hpr1;
	pState->hpl1 = hpl1;
	pState->hpr2 = hpr2;
	pState->hpl2 = hpl2;

/*	CSAMPLE lfoPeriod = m_centerFreqParameter ? m_centerFreqParameter->getValue().toDouble() : 0.0f;
	CSAMPLE lfoDepth = m_bandwidthParameter ? m_bandwidthParameter->getValue().toDouble() : 0.0f;
	CSAMPLE lfoDelay = m_boostParameter ? m_boostParameter->getValue().toDouble() : 0.0f;
	
	qDebug()<<"CF " << lfoPeriod << " BW " << lfoDepth << " boo " << lfoDelay;*/

}

ParametricEqState* ParametricEq::getStateForChannel(const QString channelId) {
    ParametricEqState* pState = NULL;
    if (!m_parametricEqStates.contains(channelId)) {
        pState = new ParametricEqState();
        /*m_parametricEqStates[channelId] = pState;
        SampleUtil::applyGain(pState->delayBuffer, 0.0f, kMaxDelay);
        pState->delayPos = 0;
        pState->time = 0;
	*/
    } else {
        pState = m_parametricEqStates[channelId];
    }
    return pState;
}
