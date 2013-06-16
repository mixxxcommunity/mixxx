/***************************************************************************
                          enginedeck.h  -  description
                             -------------------
    begin                : Sun Apr 28 2002
    copyright            : (C) 2002 by
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

#ifndef ENGINEDECK_H
#define ENGINEDECK_H

#include "engine/engineobject.h"
#include "engine/enginechannel.h"
#include "configobject.h"

class EffectsManager;
class EngineBuffer;
class EngineBuffer;
class EngineClipping;
class EngineFilterBlock;
class EngineFlanger;
class EngineMaster;
class EnginePregain;
class EngineVinylSoundEmu;
class EngineVuMeter;

class EngineDeck : public EngineChannel {
    Q_OBJECT
  public:
    EngineDeck(const char *group, ConfigObject<ConfigValue>* pConfig,
               EngineMaster* pMixingEngine,
               EngineChannel::ChannelOrientation defaultOrientation = CENTER);
    virtual ~EngineDeck();

    virtual void process(const CSAMPLE *pIn, const CSAMPLE *pOut,
                         const int iBufferSize);

    // TODO(XXX) This hack needs to be removed.
    virtual EngineBuffer* getEngineBuffer();

    virtual bool isActive();
  private:
    ConfigObject<ConfigValue>* m_pConfig;
    EngineBuffer* m_pBuffer;
    EngineClipping* m_pClipping;
    EngineFilterBlock* m_pFilter;
    EnginePregain* m_pPregain;
    EngineVinylSoundEmu* m_pVinylSoundEmu;
    EngineVuMeter* m_pVUMeter;
    EffectsManager* m_pEffectsManager;
};

#endif
