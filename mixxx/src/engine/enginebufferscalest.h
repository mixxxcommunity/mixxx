/***************************************************************************

                          enginebufferscalest.h  -  description
                             -------------------

	begin                : November 2004
	copyright            : (C) 2004 by Tue Haste Andersen
	email                : haste@diku.dk

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
  *@author Tue Haste Andersen
*/

#ifndef ENGINEBUFFERSCALEST_H
#define ENGINEBUFFERSCALEST_H

#include <QMutex>
#include "enginebufferscale.h"
#include "SoundTouch.h"

/** Number of samples to read ahead */
const int kiSoundTouchReadAheadLength = 10000;
using namespace soundtouch;

class ReadAheadManager;

/**
  * Performs time scaling of audio based on the SoundTouch library.
  */
class EngineBufferScaleST : public EngineBufferScale {
    Q_OBJECT
public:
    EngineBufferScaleST(ReadAheadManager* pReadAheadManager);
    ~EngineBufferScaleST();

    /** Toggle pitch independent time stretch */
    void setPitchIndpTimeStretch(bool b);
    bool getPitchIndpTimeStretch(void);

    /** Scale buffer */
    CSAMPLE* scale(double playpos, unsigned long buf_size,
                   CSAMPLE* pBase, unsigned long iBaseLength);

    /** Set tempo */
    double setTempo(double dTempo);

    /** Set base rate */
    void setBaseRate(double dBaseRate);

    /** Flush buffer */
    void clear();

public slots:
    void slotSetSamplerate(double dSampleRate);
private:
    /** Holds the playback direction */
    bool m_bBackwards;

    /** Buffer used to reverse output from SoundTouch
      * library when playback direction is backwards */
    CSAMPLE *buffer_back;

    /** SoundTouch time/pitch scaling lib */
    SoundTouch *m_pSoundTouch;

    /** True if in pitch independent time stretch mode */
    bool m_bPitchIndpTimeStretch;

    /** Used when clear is called */
    bool m_bClear;

    /** Used to protect SoundTouch calls */
    QMutex m_qMutex;

    // The read-ahead manager that we use to fetch samples
    ReadAheadManager* m_pReadAheadManager;
};

#endif
