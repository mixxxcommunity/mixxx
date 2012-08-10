/***************************************************************************
                          engineclipping.cpp  -  description
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

#include "engineclipping.h"
#include "controlpotmeter.h"
#include "sampleutil.h"

EngineClipping::EngineClipping() {
}

EngineClipping::~EngineClipping() {
}

void EngineClipping::process(const CSAMPLE * pIn, const CSAMPLE * pOut, const int iBufferSize) {
    static const FLOAT_TYPE kfMaxAmp = 32767.;
    // static const FLOAT_TYPE kfClip = 0.8*kfMaxAmp;

    CSAMPLE * pOutput = (CSAMPLE *)pOut;
    // SampleUtil clamps the buffer and if pIn and pOut are aliased will not copy.
    SampleUtil::copyClampBuffer(kfMaxAmp, -kfMaxAmp,
                                 pOutput, pIn, iBufferSize);
}

