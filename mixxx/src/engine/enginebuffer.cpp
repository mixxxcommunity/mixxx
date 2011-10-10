/***************************************************************************
                          enginebuffer.cpp  -  description
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

#include <QEvent>
#include <QtDebug>

#include "engine/enginebuffer.h"
#include "cachingreader.h"

#include "controlpushbutton.h"
#include "controlobjectthreadmain.h"
#include "configobject.h"
#include "controlpotmeter.h"
#include "engine/enginebufferscalest.h"
#include "engine/enginebufferscalelinear.h"
#include "engine/enginebufferscalereal.h"
#include "engine/enginebufferscaledummy.h"
#include "mathstuff.h"
#include "engine/engineworkerscheduler.h"
#include "engine/readaheadmanager.h"
#include "engine/enginecontrol.h"
#include "engine/loopingcontrol.h"
#include "engine/ratecontrol.h"
#include "engine/bpmcontrol.h"
#include "engine/quantizecontrol.h"

#ifdef __VINYLCONTROL__
#include "engine/vinylcontrolcontrol.h"
#endif

#include "trackinfoobject.h"

#ifdef _MSC_VER
#include <float.h>  // for _isnan() on VC++
#define isnan(x) _isnan(x)  // VC++ uses _isnan() instead of isnan()
#else
#include <math.h>  // for isnan() everywhere else
#endif


EngineBuffer::EngineBuffer(const char * _group, ConfigObject<ConfigValue> * _config) :
    m_engineLock(QMutex::Recursive),
    group(_group),
    m_pConfig(_config),
    m_pLoopingControl(NULL),
    m_pRateControl(NULL),
    m_pBpmControl(NULL),
    m_pReadAheadManager(NULL),
    m_pOtherEngineBuffer(NULL),
    m_pReader(NULL),
    filepos_play(0.),
    rate_old(0.),
    file_length_old(-1),
    file_srate_old(0),
    m_iSamplesCalculated(0),
    m_pTrackEnd(NULL),
    m_pRepeat(NULL),
    startButton(NULL),
    endButton(NULL),
    m_pScale(NULL),
    m_pScaleLinear(NULL),
    m_pScaleST(NULL),
    m_bScalerChanged(false),
    m_bLastBufferPaused(true),
    m_fRampValue(0.0) {

    m_fLastSampleValue[0] = 0;
    m_fLastSampleValue[1] = 0;

    m_pReader = new CachingReader(_group, _config);
    connect(m_pReader, SIGNAL(trackLoaded(TrackPointer, int, int)),
            this, SLOT(slotTrackLoaded(TrackPointer, int, int)),
            Qt::DirectConnection);
    connect(m_pReader, SIGNAL(trackLoadFailed(TrackPointer, QString)),
            this, SLOT(slotTrackLoadFailed(TrackPointer, QString)),
            Qt::DirectConnection);

    // Play button
    playButton = new ControlPushButton(ConfigKey(group, "play"));
    playButton->setToggleButton(true);
    connect(playButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlPlay(double)),
            Qt::DirectConnection);
    playButtonCOT = new ControlObjectThreadMain(playButton);

    //Play from Start Button (for sampler)
    playStartButton = new ControlPushButton(ConfigKey(group, "start_play"));
    connect(playStartButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlPlayFromStart(double)),
            Qt::DirectConnection);
    playStartButton->set(0);
    playStartButtonCOT = new ControlObjectThreadMain(playStartButton);

    //Stop playback (for sampler)
    stopButton = new ControlPushButton(ConfigKey(group, "stop"));
    connect(stopButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlStop(double)),
            Qt::DirectConnection);
    stopButton->set(0);
    stopButtonCOT = new ControlObjectThreadMain(stopButton);

    // Start button
    startButton = new ControlPushButton(ConfigKey(group, "start"));
    connect(startButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlStart(double)),
            Qt::DirectConnection);

    // End button
    endButton = new ControlPushButton(ConfigKey(group, "end"));
    connect(endButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlEnd(double)),
            Qt::DirectConnection);

    m_pMasterRate = ControlObject::getControl(ConfigKey("[Master]", "rate"));

    // Actual rate (used in visuals, not for control)
    rateEngine = new ControlObject(ConfigKey(group, "rateEngine"));

    // Slider to show and change song position
    //these bizarre choices map conveniently to the 0-127 range of midi
    playposSlider = new ControlPotmeter(ConfigKey(group, "playposition"), -0.14, 1.14);
    connect(playposSlider, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlSeek(double)),
            Qt::DirectConnection);

    // Control used to communicate ratio playpos to GUI thread
    visualPlaypos =
        new ControlPotmeter(ConfigKey(group, "visual_playposition"), -0.14, 1.14);

    // m_pTrackEnd is used to signal when at end of file during
    // playback. TODO(XXX) This should not even be a control object because it
    // is an internal flag used only by the EngineBuffer.
    m_pTrackEnd = new ControlObject(ConfigKey(group, "TrackEnd"));
    //A COTM for use in slots that are called by the GUI thread.
    m_pTrackEndCOT = new ControlObjectThreadMain(m_pTrackEnd);

    m_pRepeat = new ControlPushButton(ConfigKey(group, "repeat"));
    m_pRepeat->setToggleButton(true);

    // Sample rate
    m_pSampleRate = ControlObject::getControl(ConfigKey("[Master]","samplerate"));

    m_pTrackSamples = new ControlObject(ConfigKey(group, "track_samples"));
    m_pTrackSampleRate = new ControlObject(ConfigKey(group, "track_samplerate"));

    // Quantization Controller for enabling and disabling the
    // quantization (alignment) of loop in/out positions and (hot)cues with
    // beats.
    addControl(new QuantizeControl(_group, _config));

    // Create the Loop Controller
    m_pLoopingControl = new LoopingControl(_group, _config);
    addControl(m_pLoopingControl);

#ifdef __VINYLCONTROL__
    // If VinylControl is enabled, add a VinylControlControl. This must be done
    // before RateControl is created.
    addControl(new VinylControlControl(group, _config));
#endif

    // Create the Rate Controller
    m_pRateControl = new RateControl(_group, _config);
    addControl(m_pRateControl);

    fwdButton = ControlObject::getControl(ConfigKey(_group, "fwd"));
    backButton = ControlObject::getControl(ConfigKey(_group, "back"));

    // Create the BPM Controller
    m_pBpmControl = new BpmControl(_group, _config);
    addControl(m_pBpmControl);

    m_pReadAheadManager = new ReadAheadManager(m_pReader);
    m_pReadAheadManager->addEngineControl(m_pLoopingControl);

    // Construct scaling objects
    m_pScaleLinear = new EngineBufferScaleLinear(m_pReadAheadManager);

    m_pScaleST = new EngineBufferScaleST(m_pReadAheadManager);
    //m_pScaleST = (EngineBufferScaleST*)new EngineBufferScaleDummy(m_pReadAheadManager);
    setPitchIndpTimeStretch(false); // default to VE, let the user specify PITS in their mix

    setNewPlaypos(0.);

    m_pKeylock = new ControlPushButton(ConfigKey(group, "keylock"));
    m_pKeylock->setToggleButton(true);
    m_pKeylock->set(false);

    m_pEject = new ControlPushButton(ConfigKey(group, "eject"));
    connect(m_pEject, SIGNAL(valueChanged(double)),
            this, SLOT(slotEjectTrack(double)),
            Qt::DirectConnection);

    //m_iRampIter = 0;

    /*df.setFileName("mixxx-debug.csv");
    df.open(QIODevice::WriteOnly | QIODevice::Text);
    writer.setDevice(&df);*/
}

EngineBuffer::~EngineBuffer()
{
    //close the writer
    /*df.close();*/
    delete m_pReadAheadManager;
    delete m_pReader;

    delete playButtonCOT;
    delete playButton;
    delete playStartButtonCOT;
    delete playStartButton;
    delete startButton;
    delete endButton;
    delete stopButtonCOT;
    delete stopButton;
    delete rateEngine;
    delete playposSlider;
    delete visualPlaypos;

    delete m_pTrackEndCOT;
    delete m_pTrackEnd;

    delete m_pRepeat;

    delete m_pTrackSamples;
    delete m_pTrackSampleRate;

    delete m_pScaleLinear;
    delete m_pScaleST;

    delete m_pKeylock;
    delete m_pEject;

    while (m_engineControls.size() > 0) {
        EngineControl* pControl = m_engineControls.takeLast();
        delete pControl;
    }
}

void EngineBuffer::setPitchIndpTimeStretch(bool b)
{
    // MUST ACQUIRE THE PAUSE MUTEX BEFORE CALLING THIS METHOD

    // Change sound scale mode

    //SoundTouch's linear interpolation code doesn't sound very good.
    //Our own EngineBufferScaleLinear sounds slightly better, but it's
    //not working perfectly. Eventually we should have our own working
    //better, so scratching sounds good.

    //Update Dec 30/2007
    //If we delete the m_pScale object and recreate it, it eventually
    //causes some weird bad pointer somewhere, which will either cause
    //the waveform the roll in a weird way or fire an ASSERT from
    //visualchannel.cpp or something. Need to valgrind this or something.

    if (b == true) {
        m_pScale = m_pScaleST;
        ((EngineBufferScaleST *)m_pScaleST)->setPitchIndpTimeStretch(b);
    } else {
        m_pScale = m_pScaleLinear;
    }
    m_bScalerChanged = true;
}

double EngineBuffer::getBpm()
{
    return m_pBpmControl->getBpm();
}

void EngineBuffer::setOtherEngineBuffer(EngineBuffer * pOtherEngineBuffer)
{
    if (!m_pOtherEngineBuffer) {
        m_pOtherEngineBuffer = pOtherEngineBuffer;
        m_pBpmControl->setOtherEngineBuffer(pOtherEngineBuffer);
    } else
        qCritical("EngineBuffer: Other engine buffer already set!");
}

void EngineBuffer::setNewPlaypos(double newpos)
{
    //qDebug() << "engine new pos " << newpos;

    filepos_play = newpos;

    // Ensures that the playpos slider gets updated in next process call
    m_iSamplesCalculated = 1000000;

    // The right place to do this?
    if (m_pScale)
        m_pScale->clear();
    m_pReadAheadManager->notifySeek(filepos_play);

    // Must hold the engineLock while using m_engineControls
    m_engineLock.lock();
    for (QList<EngineControl*>::iterator it = m_engineControls.begin();
         it != m_engineControls.end(); it++) {
        EngineControl *pControl = *it;
        pControl->notifySeek(filepos_play);
    }
    m_engineLock.unlock();
}

const char * EngineBuffer::getGroup()
{
    return group;
}

double EngineBuffer::getRate()
{
    return m_pRateControl->getRawRate();
}

// WARNING: Always called from the EngineWorker thread pool
void EngineBuffer::slotTrackLoaded(TrackPointer pTrack,
                                   int iTrackSampleRate,
                                   int iTrackNumSamples) {
    pause.lock();
    m_pCurrentTrack = pTrack;
    file_srate_old = iTrackSampleRate;
    file_length_old = iTrackNumSamples;
    m_pTrackSamples->set(iTrackNumSamples);
    m_pTrackSampleRate->set(iTrackSampleRate);
    slotControlSeek(0.);

    // Let the engine know that a track is loaded now.
    m_pTrackEndCOT->slotSet(0.0f); //XXX: Not sure if to use the COT or CO here

    pause.unlock();

    emit(trackLoaded(pTrack));
}

// WARNING: Always called from the EngineWorker thread pool
void EngineBuffer::slotTrackLoadFailed(TrackPointer pTrack,
                                       QString reason) {
    ejectTrack();
    emit(trackLoadFailed(pTrack, reason));
}

TrackPointer EngineBuffer::getLoadedTrack() const {
    return m_pCurrentTrack;
}

void EngineBuffer::ejectTrack() {
    // Don't allow ejections while playing a track. We don't need to lock to
    // call ControlObject::get() so this is fine.
    if (playButton->get() > 0)
        return;

    pause.lock();
    TrackPointer pTrack = m_pCurrentTrack;
    m_pCurrentTrack.clear();
    file_srate_old = 0;
    file_length_old = 0;
    playButton->set(0.0);
    slotControlSeek(0.);
    m_pTrackSamples->set(0);
    m_pTrackSampleRate->set(0);
    pause.unlock();

    emit(trackUnloaded(pTrack));
}

// WARNING: This method runs in both the GUI thread and the Engine Thread
void EngineBuffer::slotControlSeek(double change)
{
    if(isnan(change) || change > 1.14 || change < -1.14) {
        // This seek is ridiculous.
        return;
    }

    // Find new playpos, restrict to valid ranges.
    double new_playpos = round(change*file_length_old);

    if (new_playpos > file_length_old)
        new_playpos = file_length_old;
    //if (new_playpos < 0.)
    //   new_playpos = 0.;

    // Ensure that the file position is even (remember, stereo channel files...)
    if (!even((int)new_playpos))
        new_playpos--;

    // Give EngineControl's a chance to veto or correct the seek target.

    // Seek reader
    Hint seek_hint;
    seek_hint.sample = new_playpos;
    seek_hint.length = 0;
    seek_hint.priority = 1;
    QList<Hint> hint_list;
    hint_list.append(seek_hint);
    m_pReader->hintAndMaybeWake(hint_list);
    setNewPlaypos(new_playpos);
}

// WARNING: This method runs in both the GUI thread and the Engine Thread
void EngineBuffer::slotControlSeekAbs(double abs)
{
    slotControlSeek(abs/file_length_old);
}

void EngineBuffer::slotControlPlay(double v)
{
    // If no track is currently loaded, turn play off.
    if (v > 0.0 && !m_pCurrentTrack) {
        playButton->set(0.0f);
    }
}

void EngineBuffer::slotControlStart(double v)
{
    if (v > 0.0) {
        slotControlSeek(0.);
    }
}

void EngineBuffer::slotControlEnd(double v)
{
    if (v > 0.0) {
        slotControlSeek(1.);
    }
}

void EngineBuffer::slotControlPlayFromStart(double v)
{
    if (v > 0.0) {
        slotControlSeek(0.);
        playButton->set(1);
    }
}

void EngineBuffer::slotControlStop(double v)
{
    if (v > 0.0) {
        playButton->set(0);
    }
}

void EngineBuffer::process(const CSAMPLE *, const CSAMPLE * pOut, const int iBufferSize)
{

    m_pReader->process();
    // Steps:
    // - Lookup new reader information
    // - Calculate current rate
    // - Scale the audio with m_pScale, copy the resulting samples into the
    //   output buffer
    // - Give EngineControl's a chance to do work / request seeks, etc
    // - Process repeat mode if we're at the end or beginning of a track
    // - Set last sample value (m_fLastSampleValue) so that rampOut works? Other
    //   miscellaneous upkeep issues.

    CSAMPLE * pOutput = (CSAMPLE *)pOut;

    bool bCurBufferPaused = false;
    double rate;

    if (!m_pTrackEnd->get() && pause.tryLock()) {

        if (m_pKeylock->get() && m_pScale != m_pScaleST) {
            setPitchIndpTimeStretch(true);
        } else if (!m_pKeylock->get() && m_pScale == m_pScaleST) {
            setPitchIndpTimeStretch(false);
        }

        float sr = m_pSampleRate->get();

        double baserate = 0.0f;
        if (sr > 0)
            baserate = ((double)file_srate_old/sr);

        bool paused = playButton->get() != 0.0f ? false : true;

        rate = m_pRateControl->calculateRate(baserate, paused, iBufferSize);
        //qDebug() << "rate" << rate << " paused" << paused;

        // If the rate has changed, set it in the scale object
        if (rate != rate_old || m_bScalerChanged) {
            // The rate returned by the scale object can be different from the wanted rate!
            // Make sure new scaler has proper position
            if (m_bScalerChanged) {
                setNewPlaypos(filepos_play);
            } else if (m_pScale != m_pScaleLinear) { //linear scaler does this part for us now
                //XXX: Trying to force RAMAN to read from correct
                //     playpos when rate changes direction - Albert
                if ((rate_old <= 0 && rate > 0) ||
                    (rate_old >= 0 && rate < 0)) {
                    setNewPlaypos(filepos_play);
                }
            }

            rate_old = rate;
            if (baserate > 0) //Prevent division by 0
                rate = baserate*m_pScale->setTempo(rate/baserate);
            m_pScale->setBaseRate(baserate);
            rate_old = rate;
            // Scaler is up to date now.
            m_bScalerChanged = false;
        }

        bool at_start = filepos_play <= 0;
        bool at_end = filepos_play >= file_length_old;
        bool backwards = rate < 0;

        // If we're playing past the end, playing before the start, or standing
        // still then by definition the buffer is paused.
        bCurBufferPaused = rate == 0 ||
            //(at_start && backwards) ||
            (at_end && !backwards);


        // If the buffer is not paused, then scale the audio.
        if (!bCurBufferPaused) {
            CSAMPLE *output;
            double idx;

            Q_ASSERT(even(iBufferSize));

            // The fileposition should be: (why is this thing a double anyway!?
            // Integer valued.
            Q_ASSERT(round(filepos_play) == filepos_play);
            // Even.
            Q_ASSERT(even(filepos_play));

            // Perform scaling of Reader buffer into buffer.
            output = m_pScale->scale(0,
                                     iBufferSize,
                                     0,
                                     0);
            idx = m_pScale->getNewPlaypos();

            // qDebug() << "sourceSamples used " << iSourceSamples
            //          <<" idx " << idx
            //          << ", buffer pos " << iBufferStartSample
            //          << ", play " << filepos_play
            //          << " bufferlen " << iBufferSize;

            // Copy scaled audio into pOutput
            memcpy(pOutput, output, sizeof(CSAMPLE) * iBufferSize);

            // for(int i=0; i<iBufferSize; i++) {
            //     pOutput[i] = output[i];
            // }

            // Adjust filepos_play by the amount we processed.
            filepos_play += idx;
            //filepos_play = math_max(0, filepos_play);
            //// We need the above protection against negative playpositions
            //// in case SoundTouch/EngineBufferSoundTouch gives us too many samples.

            // Get rid of annoying decimals that the scaler sometimes produces
            filepos_play = round(filepos_play);

            if (!even(filepos_play))
                filepos_play--;

        } // else (bCurBufferPaused)

        m_engineLock.lock();
        QListIterator<EngineControl*> it(m_engineControls);
        while (it.hasNext()) {
            EngineControl* pControl = it.next();
            pControl->setCurrentSample(filepos_play, file_length_old);
            double control_seek = pControl->process(rate, filepos_play,
                                                    file_length_old, iBufferSize);

            if (control_seek != kNoTrigger) {
                filepos_play = control_seek;
                Q_ASSERT(round(filepos_play) == filepos_play);

                // Safety check that the EngineControl didn't pass us a bogus
                // value
                if (!even(filepos_play))
                    filepos_play--;

                // Fix filepos_play so that it is not out of bounds.
                if (file_length_old > 0) {
                    if(filepos_play > file_length_old) {
                        filepos_play = file_length_old;
                        at_end = true;
                    } else if(filepos_play < 0) {
                        filepos_play = 0;
                        at_start = true;
                    }
                }
                // TODO(XXX) need to re-evaluate this later. If we
                // setNewPlaypos, that clear()'s soundtouch, which might screw
                // up the audio. This sort of jump is a normal event. Also, the
                // EngineControl which caused this jump will get a notifySeek
                // for the same jump which might be confusing. For 1.8.0
                // purposes this works fine. If we do not notifySeek the RAMAN,
                // the engine and RAMAN can get out of sync.

                //setNewPlaypos(filepos_play);
                m_pReadAheadManager->notifySeek(filepos_play);
            }
        }
        m_engineLock.unlock();


        // Update all the indicators that EngineBuffer publishes to allow
        // external parts of Mixxx to observe its status.
        updateIndicators(rate, iBufferSize);

        // Handle repeat mode
        at_start = filepos_play <= 0;
        at_end = filepos_play >= file_length_old;

        bool repeat_enabled = m_pRepeat->get() != 0.0f;

        bool end_of_track = //(at_start && backwards) ||
            (at_end && !backwards);

        // If playbutton is pressed, check if we are at start or end of track
        if ((playButton->get() || (fwdButton->get() || backButton->get()))
            && end_of_track) {
            if (repeat_enabled) {
                double seekPosition = at_start ? file_length_old : 0;
                slotControlSeek(seekPosition);
            } else {
                playButton->set(0.);
            }
        }

        // release the pauselock
        pause.unlock();
    } else { // if (!m_pTrackEnd->get() && pause.tryLock()) {
        // If we can't get the pause lock then this buffer will be silence.
        bCurBufferPaused = true;
    }

    // Give the Reader hints as to which chunks of the current song we
    // really care about. It will try very hard to keep these in memory
    hintReader(rate, iBufferSize);

    if (m_bLastBufferPaused && !bCurBufferPaused) {
        if (fabs(rate) > 0.005) //at very slow forward rates, don't ramp up
            m_iRampState = ENGINE_RAMP_UP;
    } else if (!m_bLastBufferPaused && bCurBufferPaused) {
        m_iRampState = ENGINE_RAMP_DOWN;
    } else { //we are not changing state
        //make sure we aren't accidentally ramping down
        //this is how we make sure that ramp value will become 1.0 eventually
        if (fabs(rate) > 0.005 && m_iRampState != ENGINE_RAMP_UP && m_fRampValue < 1.0)
            m_iRampState = ENGINE_RAMP_UP;
    }

    //let's try holding the last sample value constant, and pull it
    //towards zero
    float ramp_inc = 0;
    if (m_iRampState == ENGINE_RAMP_UP) {
        ramp_inc = (m_iRampState * 0.2) / iBufferSize; //ramp up quickly (5 frames)
    } else if (m_iRampState == ENGINE_RAMP_DOWN) {
        ramp_inc = (m_iRampState * 0.08) / iBufferSize; //but down slowly
    }

    //float fakerate = rate * 30000 == 0 ? -5000 : rate*30000;
    for (int i=0; i<iBufferSize; i+=2) {
        if (bCurBufferPaused) {
            float dither = (float)(rand() % 32768) / 32768 - 0.5; // dither
            pOutput[i] = m_fLastSampleValue[0] * m_fRampValue + dither;
            pOutput[i+1] = m_fLastSampleValue[1] * m_fRampValue + dither;
        } else {
            pOutput[i] = pOutput[i] * m_fRampValue;
            pOutput[i+1] = pOutput[i+1] * m_fRampValue;
        }

        //writer << pOutput[i] <<  "\n";
        m_fRampValue += ramp_inc;
        if (m_fRampValue >= 1.0) {
            m_iRampState = ENGINE_RAMP_NONE;
            m_fRampValue = 1.0;
        }
        if (m_fRampValue <= 0.0) {
            m_iRampState = ENGINE_RAMP_NONE;
            m_fRampValue = 0.0;
        }
    }

    if ((!bCurBufferPaused && m_iRampState == ENGINE_RAMP_NONE) ||
        (bCurBufferPaused && m_fRampValue == 0.0)) {
        m_fLastSampleValue[0] = pOutput[iBufferSize-2];
        m_fLastSampleValue[1] = pOutput[iBufferSize-1];
    }

    m_bLastBufferPaused = bCurBufferPaused;
}


void EngineBuffer::rampOut(const CSAMPLE* pOut, int iBufferSize)
{
    CSAMPLE * pOutput = (CSAMPLE *)pOut;

    //qDebug() << "ramp out";

    // Ramp to zero
    int i=0;
    if (m_fLastSampleValue[0]!=0.) {
        // TODO(XXX) SSE
        if (pOutput[0] == 0) {
            while (i<iBufferSize) {
                float sigmoid = sigmoid_zero((float)(iBufferSize - i), (float)iBufferSize);
                float dither = (float)(rand() % 32768) / 32768 - 0.5; // dither
                pOutput[i] = (float)m_fLastSampleValue[0] * sigmoid + dither;
                pOutput[i+1] = (float)m_fLastSampleValue[1] * sigmoid + dither;
                i+=2;
            }
        } else {
            while (i<iBufferSize) {
                float sigmoid = sigmoid_zero((float)(iBufferSize - i), (float)iBufferSize);
                float dither = (float)(rand() % 32768) / 32768 - 0.5; // dither
                pOutput[i] = (float)pOutput[i] * sigmoid + dither;
                pOutput[i+1] = (float)pOutput[i+1] * sigmoid + dither;
                i+=2;
               }
        }
    }

    // TODO(XXX) memset
    // Reset rest of buffer
    while (i<iBufferSize)
    {
        pOutput[i]=0.;
        ++i;
    }
}

void EngineBuffer::updateIndicators(double rate, int iBufferSize) {

    // Increase samplesCalculated by the buffer size
    m_iSamplesCalculated += iBufferSize;

    double fFractionalPlaypos = 0.0;
    if (file_length_old!=0.) {
        fFractionalPlaypos = math_min(filepos_play,file_length_old);
        fFractionalPlaypos /= file_length_old;
    } else {
        fFractionalPlaypos = 0.;
    }

    // Update indicators that are only updated after every
    // sampleRate/kiUpdateRate samples processed.  (e.g. playposSlider,
    // rateEngine)
    if (m_iSamplesCalculated > (m_pSampleRate->get()/kiUpdateRate)) {
        playposSlider->set(fFractionalPlaypos);

        if(rate != rateEngine->get())
            rateEngine->set(rate);

        // Reset sample counter
        m_iSamplesCalculated = 0;
    }

    // Update visual control object, this needs to be done more often than the
    // rateEngine and playpos slider
    visualPlaypos->set(fFractionalPlaypos);
}

void EngineBuffer::hintReader(const double dRate,
                              const int iSourceSamples) {
    m_engineLock.lock();

    m_hintList.clear();
    m_pReadAheadManager->hintReader(dRate, m_hintList, iSourceSamples);

    QListIterator<EngineControl*> it(m_engineControls);
    while (it.hasNext()) {
        EngineControl* pControl = it.next();
        pControl->hintReader(m_hintList);
    }
    m_pReader->hintAndMaybeWake(m_hintList);

    m_engineLock.unlock();
}

// WARNING: This method runs in the GUI thread
void EngineBuffer::slotLoadTrack(TrackPointer pTrack) {
    // Raise the track end flag so the EngineBuffer stops processing frames
    m_pTrackEndCOT->slotSet(1.0);

    //Stop playback
    playButtonCOT->slotSet(0.0);

    // Signal to the reader to load the track. The reader will respond with
    // either trackLoaded or trackLoadFailed signals.
    m_pReader->newTrack(pTrack);
    m_pReader->wake();
}

void EngineBuffer::addControl(EngineControl* pControl) {
    // Connect to signals from EngineControl here...
    m_engineLock.lock();
    m_engineControls.push_back(pControl);
    m_engineLock.unlock();
    connect(pControl, SIGNAL(seek(double)),
            this, SLOT(slotControlSeek(double)),
            Qt::DirectConnection);
    connect(pControl, SIGNAL(seekAbs(double)),
            this, SLOT(slotControlSeekAbs(double)),
            Qt::DirectConnection);
    connect(this, SIGNAL(trackLoaded(TrackPointer)),
            pControl, SLOT(trackLoaded(TrackPointer)),
            Qt::DirectConnection);
    connect(this, SIGNAL(trackUnloaded(TrackPointer)),
            pControl, SLOT(trackUnloaded(TrackPointer)),
            Qt::DirectConnection);
}

void EngineBuffer::bindWorkers(EngineWorkerScheduler* pWorkerScheduler) {
    pWorkerScheduler->bindWorker(m_pReader);
}

bool EngineBuffer::isTrackLoaded() {
    if (m_pCurrentTrack) {
        return true;
    }
    return false;
}

void EngineBuffer::slotEjectTrack(double v) {
    if (v > 0) {
        ejectTrack();
    }
}
