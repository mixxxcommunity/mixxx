/***************************************************************************
                          enginebuffer.h  -  description
                             -------------------
    begin                : Wed Feb 20 2002
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

#ifndef ENGINEBUFFER_H
#define ENGINEBUFFER_H

#include <qapplication.h>
#include <QMutex>
#include "defs.h"
#include "engine/engineobject.h"
#include "trackinfoobject.h"
#include "configobject.h"
#include "rotary.h"
//for the writer
//#include <QtCore>

class EngineControl;
class BpmControl;
class RateControl;
class LoopingControl;
class ReadAheadManager;
class ControlObject;
class ControlPushButton;
class ControlObjectThreadMain;
class ControlBeat;
class ControlTTRotary;
class ControlPotmeter;
class CachingReader;
class EngineBufferScale;
class EngineBufferScaleLinear;
class EngineBufferScaleST;
class EngineWorkerScheduler;

struct Hint;

/**
  *@author Tue and Ken Haste Andersen
*/

// Length of audio beat marks in samples
const int audioBeatMarkLen = 40;

// Temporary buffer length
const int kiTempLength = 200000;

// Rate at which the playpos slider is updated (using a sample rate of 44100 Hz):
const int kiUpdateRate = 10;

// End of track mode constants
const int TRACK_END_MODE_STOP = 0;
const int TRACK_END_MODE_NEXT = 1;
const int TRACK_END_MODE_LOOP = 2;
const int TRACK_END_MODE_PING = 3;

//vinyl status constants
//XXX: move this to vinylcontrol.h once thread startup is moved
const int VINYL_STATUS_DISABLED = 0;
const int VINYL_STATUS_OK = 1;
const int VINYL_STATUS_WARNING = 2;
const int VINYL_STATUS_ERROR = 3;

const int ENGINE_RAMP_DOWN = -1;
const int ENGINE_RAMP_NONE = 0;
const int ENGINE_RAMP_UP = 1;

//const int kiRampLength = 3;

class EngineBuffer : public EngineObject
{
     Q_OBJECT
public:
    EngineBuffer(const char *_group, ConfigObject<ConfigValue> *_config);
    ~EngineBuffer();
    bool getPitchIndpTimeStretch(void);

    void bindWorkers(EngineWorkerScheduler* pWorkerScheduler);

    // Add an engine control to the EngineBuffer
    void addControl(EngineControl* pControl);

    /** Return the current rate (not thread-safe) */
    double getRate();
    /** Returns current bpm value (not thread-safe) */
    double getBpm();
    /** Sets pointer to other engine buffer/channel */
    void setOtherEngineBuffer(EngineBuffer *);

    /** Reset buffer playpos and set file playpos. This must only be called
      * while holding the pause mutex */
    void setNewPlaypos(double);

    void process(const CSAMPLE *pIn, const CSAMPLE *pOut, const int iBufferSize);

    const char* getGroup();
    bool isTrackLoaded();
    TrackPointer getLoadedTrack() const;

  public slots:
    void slotControlPlay(double);
    void slotControlPlayFromStart(double);
    void slotControlStop(double);
    void slotControlStart(double);
    void slotControlEnd(double);
    void slotControlSeek(double);
    void slotControlSeekAbs(double);

    // Request that the EngineBuffer load a track. Since the process is
    // asynchronous, EngineBuffer will emit a trackLoaded signal when the load
    // has completed.
    void slotLoadTrack(TrackPointer pTrack);

    void slotEjectTrack(double);

  signals:
    void trackLoaded(TrackPointer pTrack);
    void trackLoadFailed(TrackPointer pTrack, QString reason);
    void trackUnloaded(TrackPointer pTrack);

  private slots:
    void slotTrackLoaded(TrackPointer pTrack,
                         int iSampleRate, int iNumSamples);
    void slotTrackLoadFailed(TrackPointer pTrack,
                             QString reason);

private:
    void setPitchIndpTimeStretch(bool b);
    /** Called from process() when an empty buffer, possible ramped to zero is needed */
    void rampOut(const CSAMPLE *pOut, int iBufferSize);

    void updateIndicators(double rate, int iBufferSize);

    void hintReader(const double rate,
                    const int iSourceSamples);

    void ejectTrack();

    // Lock for modifying local engine variables that are not thread safe, such
    // as m_engineControls and m_hintList
    QMutex m_engineLock;

    /** Holds the name of the control group */
    const char* group;
    ConfigObject<ConfigValue>* m_pConfig;

    /** Pointer to the loop control object */
    LoopingControl* m_pLoopingControl;

    /** Pointer to the rate control object */
    RateControl* m_pRateControl;

    /** Pointer to the BPM control object */
    BpmControl* m_pBpmControl;

    QList<EngineControl*> m_engineControls;

    /** The read ahead manager for EngineBufferScale's that need to read
        ahead */
    ReadAheadManager* m_pReadAheadManager;

    /** Pointer to other EngineBuffer */
    EngineBuffer* m_pOtherEngineBuffer;

    // The reader used to read audio files
    CachingReader* m_pReader;

    // List of hints to provide to the CachingReader
    QList<Hint> m_hintList;

    /** The current sample to play in the file. */
    double filepos_play;
    /** Copy of rate_exchange, used to check if rate needs to be updated */
    double rate_old;
    /** Copy of length of file */
    long int file_length_old;
    /** Copy of file sample rate*/
    int file_srate_old;
    /** Mutex controlling weather the process function is in pause mode. This happens
      * during seek and loading of a new track */
    QMutex pause;
    /** Used in update of playpos slider */
    int m_iSamplesCalculated;

    ControlObject* m_pTrackSamples;
    ControlObject* m_pTrackSampleRate;

    ControlPushButton *playButton, *buttonBeatSync, *playStartButton, *stopButton;
    ControlObjectThreadMain *playButtonCOT, *playStartButtonCOT, *m_pTrackEndCOT, *stopButtonCOT;
    ControlObject *fwdButton, *backButton;

    ControlObject *rateEngine;
    ControlObject *m_pMasterRate;
    ControlPotmeter *playposSlider;
    ControlPotmeter *visualPlaypos;
    ControlObject *m_pPlayPosSamples;
    ControlObject *m_pSampleRate;
    ControlPushButton *m_pKeylock;

    ControlPushButton *m_pEject;

    /** Control used to signal when at end of file */
    ControlObject *m_pTrackEnd;

    // Whether or not to repeat the track when at the end
    ControlPushButton* m_pRepeat;

    ControlObject *m_pVinylStatus;  //Status of vinyl control
    ControlObject *m_pVinylSeek;

    /** Fwd and back controls, start and end of track control */
    ControlPushButton *startButton, *endButton;

    /** Object used to perform waveform scaling (sample rate conversion) */
    EngineBufferScale *m_pScale;
    /** Object used for linear interpolation scaling of the audio */
    EngineBufferScaleLinear *m_pScaleLinear;
    /** Object used for pitch-indep time stretch (key lock) scaling of the audio */
    EngineBufferScaleST *m_pScaleST;
    // Indicates whether the scaler has changed since the last process()
    bool m_bScalerChanged;

    /** Holds the last sample value of the previous buffer. This is used when ramping to
      * zero in case of an immediate stop of the playback */
    float m_fLastSampleValue[2];
    /** Is true if the previous buffer was silent due to pausing */
    bool m_bLastBufferPaused;
    float m_fRampValue;
    int m_iRampState;
    //int m_iRampIter;

    TrackPointer m_pCurrentTrack;
    /*QFile df;
    QTextStream writer;*/
};

#endif
