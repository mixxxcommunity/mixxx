/***************************************************************************
                          enginepregain.cpp  -  description
                             -------------------
    copyright            : (C) 2002 by Tue and Ken Haste Andersen
    email                :
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <time.h>   // for clock() and CLOCKS_PER_SEC
#include <QtDebug>

#include "engine/enginepregain.h"

#include "controllogpotmeter.h"
#include "controlpotmeter.h"
#include "controlpushbutton.h"
#include "configobject.h"
#include "controlobject.h"
#include "engine/enginestate.h"
#include "sampleutil.h"

EnginePregain::EnginePregain(const char * group, EngineState* pEngineState) {
    CallbackControlManager* pCallbackControlManager =
            pEngineState->getControlManager();
    potmeterPregain = pCallbackControlManager->addControl(
        new ControlLogpotmeter(ConfigKey(group, "pregain"), 4.), 1);
    m_pControlReplayGain = pCallbackControlManager->addControl(
        new ControlObject(ConfigKey(group, "replaygain")), 1);
    m_pTotalGain = pCallbackControlManager->addControl(
        new ControlObject(ConfigKey(group, "total_gain")), 1);

    m_pReplayGainBoost = pCallbackControlManager->getControl(
        ConfigKey("[ReplayGain]", "InitialReplayGainBoost"));
    if (m_pReplayGainBoost == NULL) {
        m_pReplayGainBoost = pCallbackControlManager->addControl(
            new ControlPotmeter(
                ConfigKey("[ReplayGain]", "InitialReplayGainBoost"), 0., 15.),
            1);
        // Set the parent as the creator so that it is destroyed when this
        // EnginePregain is destroyed.
        m_pReplayGainBoost->setParent(this);
    }

    m_pEnableReplayGain = pCallbackControlManager->getControl(
        ConfigKey("[ReplayGain]", "ReplayGainEnabled"));
    if (m_pEnableReplayGain == NULL) {
        m_pEnableReplayGain = pCallbackControlManager->addControl(
            new ControlObject(
                ConfigKey("[ReplayGain]", "ReplayGainEnabled")), 1);
        // Set the parent as the creator so that it is destroyed when this
        // EnginePregain is destroyed.
        m_pEnableReplayGain->setParent(this);
    }

    m_bSmoothFade = false;
    m_fClock = 0;
    m_fSumClock = 0;
}

EnginePregain::~EnginePregain() {
    delete potmeterPregain;
    delete m_pControlReplayGain;
    delete m_pTotalGain;
}

void EnginePregain::process(const CSAMPLE * pIn, const CSAMPLE * pOut,
                            const int iBufferSize) {
    float fEnableReplayGain = m_pEnableReplayGain->get();
    float fReplayGainBoost = m_pReplayGainBoost->get();
    CSAMPLE * pOutput = (CSAMPLE *)pOut;
    float fGain = potmeterPregain->get();
    float fReplayGain = m_pControlReplayGain->get();
    m_fReplayGainCorrection=1;
    // TODO(XXX) Why do we do this? Removing it results in clipping at unity
    // gain so I think it was trying to compensate for some issue when we added
    // replaygain but even at unity gain (no RG) we are clipping. rryan 5/2012
    fGain = fGain/2;
    if(fReplayGain*fEnableReplayGain != 0)
    {
        // Here is the point, when ReplayGain Analyser takes its action, suggested gain changes from 0 to a nonzero value
        // We want to smoothly fade to this last.
        // Anyway we have some the problem that code cannot block the full process for one second.
        // So we need to alter gain each time ::process is called.

        if(m_bSmoothFade)//This means that a ReplayGain value has been calculated after the track has been loaded
        {
            if(m_fClock==0)
                m_fClock=clock();
            m_fSumClock += (float)((clock()-m_fClock)/CLOCKS_PER_SEC);
            m_fClock=clock();
            if(m_fSumClock<1)
            {
                //Fade smoothly

                m_fReplayGainCorrection=(1-m_fSumClock)+(m_fSumClock)*fReplayGain*pow(10, fReplayGainBoost/20);

            }
            else
            {
                m_bSmoothFade = false;
            }
        }
        else
        {
            //Passing a user defined boost
            m_fReplayGainCorrection=fReplayGain*pow(10, fReplayGainBoost/20);
        }
    }
    else
    {
        // If track has not ReplayGain value and ReplayGain is enabled
        // we prepare for smoothfading to ReplayGain suggested gain
        if(fEnableReplayGain != 0)
        {
            m_bSmoothFade=true;
            m_fClock=0;
            m_fSumClock=0;
        }
    }
    fGain = fGain*m_fReplayGainCorrection;
    m_pTotalGain->set(fGain);

    // Clamp gain to within [0, 2.0] to prevent insane gains. This can happen
    // (some corrupt files get really high replaygain values).
    fGain = math_max(0.0, math_min(2.0, fGain));

    //qDebug()<<"Clock"<<(float)clock()/CLOCKS_PER_SEC;
    // SampleUtil deals with aliased buffers and gains of 1 or 0.
    SampleUtil::copyWithGain(pOutput, pIn, fGain, iBufferSize);
}
