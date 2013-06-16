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

#include "defs.h"
#include "engine/engineobject.h"
#include "trackinfoobject.h"
#include "configobject.h"
#include "rotary.h"
#include "util/fifo.h"

//for the writer
#ifdef __SCALER_DEBUG__
#include <QtCore>
#endif

class EngineControl;
class BpmControl;
class RateControl;
class LoopingControl;
class ReadAheadManager;
class CallbackControl;
class EngineState;
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
// Number of kiUpdateRates that go by before we update BPM.
const int kiBpmUpdateRate = 40 / kiUpdateRate; //about 2.5 updates per sec

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

struct EnginePlayerMessage {
    enum Type {
        TRACK_LOADED,
        TRACK_LOAD_FAILED_NOT_FOUND,
        TRACK_LOAD_FAILED_COULDNT_OPEN,
        TRACK_UNLOADED
    };
    Type m_type;
};

struct PlayerMessage {
    enum Type {
        LOAD_TRACK
    };
    Type m_type;
    TrackPointer* m_track;
};

class EngineBuffer : public EngineObject {
     Q_OBJECT
public:
    EngineBuffer(const char *_group, ConfigObject<ConfigValue> *_config,
                 EngineState* pEngineState);
    virtual ~EngineBuffer();
    bool getPitchIndpTimeStretch(void);

    void bindWorkers(EngineWorkerScheduler* pWorkerScheduler);

    TwoWayMessagePipe<PlayerMessage, EnginePlayerMessage>& getMessagePipe() {
        return m_messagePipe;
    }

    // Add an engine control to the EngineBuffer. Only call this during startup.
    void addControl(EngineControl* pControl);

    /** Return the current rate (not thread-safe) */
    double getRate();
    /** Returns current bpm value (not thread-safe) */
    double getBpm();
    /** Sets pointer to other engine buffer/channel */
    void setOtherEngineBuffer(EngineBuffer *);

    /** Reset buffer playpos and set file playpos. */
    void setNewPlaypos(double);

    void process(const CSAMPLE *pIn, const CSAMPLE *pOut, const int iBufferSize);

    const char* getGroup();
    bool isTrackLoaded();
    TrackPointer getLoadedTrack() const;

    // For dependency injection of readers.
    void setReader(CachingReader* pReader);

  public slots:
    void slotControlPlay(double);
    void slotControlPlayFromStart(double);
    void slotControlJumpToStartAndStop(double);
    void slotControlStop(double);
    void slotControlStart(double);
    void slotControlEnd(double);
    void slotControlSeek(double);
    void slotControlSeekAbs(double);

    // Request that the EngineBuffer load a track. Since the process is
    // asynchronous, EngineBuffer will emit a trackLoaded signal when the load
    // has completed. This method is called from the GUI thread and should not
    // touch any EngineBuffer state other than thread-safe calls to m_pReader
    // and QAtomicInts.
    void slotLoadTrackFromGuiThread(TrackPointer pTrack);

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

    void updateIndicators(double rate, int iBufferSize);

    void hintReader(const double rate,
                    const int iSourceSamples);

    void ejectTrack();

    TwoWayMessagePipe<PlayerMessage, EnginePlayerMessage> m_messagePipe;

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
    /** Used in update of playpos slider */
    int m_iSamplesCalculated;
    int m_iUiSlowTick;

    CallbackControl* m_pTrackSamples;
    CallbackControl* m_pTrackSampleRate;

    CallbackControl* playButton;
    CallbackControl* fwdButton;
    CallbackControl* backButton;

    CallbackControl* rateEngine;
    CallbackControl* playposSlider;
    CallbackControl* visualPlaypos;
    CallbackControl* visualBpm;
    CallbackControl* m_pSampleRate;
    CallbackControl* m_pKeylock;

    QAtomicInt m_iTrackLoading;

    // Whether or not to repeat the track when at the end
    CallbackControl* m_pRepeat;

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

#ifdef __SCALER_DEBUG__
    QFile df;
    QTextStream writer;
#endif

    CSAMPLE* m_pDitherBuffer;
    unsigned int m_iDitherBufferReadIndex;
};

#endif
