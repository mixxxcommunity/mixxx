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
#include <QThread>

#include "engine/enginebuffer.h"
#include "cachingreader.h"
#include "sampleutil.h"


#include "controlpushbutton.h"
#include "configobject.h"
#include "controlpotmeter.h"
#include "engine/callbackcontrolmanager.h"
#include "engine/enginestate.h"
#include "engine/enginebufferscalest.h"
#include "engine/enginebufferscalelinear.h"
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

const double kMaxPlayposRange = 1.14;
const double kMinPlayposRange = -0.14;

namespace {
const bool sDebug = true;

// This is a cheap hack to allow us to assert that we are running all of our
// code in the callback thread after startup. During startup (and up until the
// first callback) g_engineThread will be NULL since we have not received a
// callback yet. EngineBuffer::process() sets g_engineThread on the first
// callback. It's fine this is global since every EngineBuffer runs in the same
// callback thread.
QThread* g_engineThread = NULL;
#define ASSERT_ENGINE_THREAD                                \
    if (sDebug && g_engineThread != NULL) {                   \
        Q_ASSERT(QThread::currentThread() == g_engineThread); \
    }

}  // namespace

EngineBuffer::EngineBuffer(const char * _group, ConfigObject<ConfigValue> * _config,
                           EngineState* pEngineState) :
    m_messagePipe(4096, 4096),
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
    m_iUiSlowTick(0),
    m_pRepeat(NULL),
    m_pScale(NULL),
    m_pScaleLinear(NULL),
    m_pScaleST(NULL),
    m_bScalerChanged(false),
    m_bLastBufferPaused(true),
    m_fRampValue(0.0),
    m_iRampState(ENGINE_RAMP_NONE),
    m_pDitherBuffer(new CSAMPLE[MAX_BUFFER_LEN]),
    m_iDitherBufferReadIndex(0) {

    // Generate dither values
    for (int i = 0; i < MAX_BUFFER_LEN; ++i) {
        m_pDitherBuffer[i] = static_cast<float>(rand() % 32768) / 32768.0 - 0.5;
    }

    m_fLastSampleValue[0] = 0;
    m_fLastSampleValue[1] = 0;

    CallbackControlManager* pCallbackControlManager =
            pEngineState->getControlManager();

    m_pReader = new CachingReader(_group, _config);
    connect(m_pReader, SIGNAL(trackLoaded(TrackPointer, int, int)),
            this, SLOT(slotTrackLoaded(TrackPointer, int, int)),
            Qt::DirectConnection);
    connect(m_pReader, SIGNAL(trackLoadFailed(TrackPointer, QString)),
            this, SLOT(slotTrackLoadFailed(TrackPointer, QString)),
            Qt::DirectConnection);

    // Play button
    ControlPushButton* pPlayButton = new ControlPushButton(
        ConfigKey(group, "play"));
    pPlayButton->setButtonMode(ControlPushButton::TOGGLE);

    playButton = pCallbackControlManager->addControl(pPlayButton, 1);
    connect(playButton, SIGNAL(valueChanged(double, double)),
            this, SLOT(slotControlPlay(double)),
            Qt::DirectConnection);

    // Play from Start Button
    CallbackControl *pStartPlayButton =
            pCallbackControlManager->addControl(
                new ControlPushButton(ConfigKey(group, "start_play")), 1);
    pStartPlayButton->setParent(this);
    connect(pStartPlayButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlPlayFromStart(double)),
            Qt::DirectConnection);

    // Jump to start and stop button
    CallbackControl* pStartStopButton =
            pCallbackControlManager->addControl(new ControlPushButton(
        ConfigKey(group, "start_stop")), 1);
    pStartStopButton->setParent(this);
    connect(pStartStopButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlJumpToStartAndStop(double)),
            Qt::DirectConnection);

    // Stop playback
    CallbackControl* pStopButton = pCallbackControlManager->addControl(
        new ControlPushButton(ConfigKey(group, "stop")), 1);
    pStopButton->setParent(this);
    connect(pStopButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlStop(double)),
            Qt::DirectConnection);

    // Start button
    CallbackControl* pStartButton = pCallbackControlManager->addControl(
        new ControlPushButton(ConfigKey(group, "start")), 1);
    pStartButton->setParent(this);
    connect(pStartButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlStart(double)),
            Qt::DirectConnection);

    // End button
    CallbackControl* pEndButton = pCallbackControlManager->addControl(
        new ControlPushButton(ConfigKey(group, "end")), 1);
    pEndButton->setParent(this);
    connect(pEndButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlEnd(double)),
            Qt::DirectConnection);

    // Eject button
    CallbackControl* pEjectButton = pCallbackControlManager->addControl(
        new ControlPushButton(ConfigKey(group, "eject")), 1);
    pEjectButton->setParent(this);
    connect(pEjectButton, SIGNAL(valueChanged(double)),
            this, SLOT(slotEjectTrack(double)),
            Qt::DirectConnection);

    // Actual rate (used in visuals, not for control)
    rateEngine = pCallbackControlManager->addControl(
        new ControlObject(ConfigKey(group, "rateEngine")), 1);

    // BPM to display in the UI (updated more slowly than the actual bpm)
    visualBpm = pCallbackControlManager->addControl(
        new ControlObject(ConfigKey(group, "visual_bpm")), 1);

    // Slider to show and change song position these bizarre choices map
    // conveniently to the 0-127 range of midi
    playposSlider = pCallbackControlManager->addControl(
        new ControlPotmeter(ConfigKey(group, "playposition"),
                            kMinPlayposRange, kMaxPlayposRange), 1);
    connect(playposSlider, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlSeek(double)),
            Qt::DirectConnection);

    // Control used to communicate ratio playpos to GUI thread
    visualPlaypos = pCallbackControlManager->addControl(
        new ControlPotmeter(ConfigKey(group, "visual_playposition"),
                            kMinPlayposRange, kMaxPlayposRange), 1);

    // Signals when we are at the end of file during playback.
    m_iTrackLoading = 0;

    ControlPushButton* pRepeatButton = new ControlPushButton(ConfigKey(group, "repeat"));
    pRepeatButton->setButtonMode(ControlPushButton::TOGGLE);
    m_pRepeat = pCallbackControlManager->addControl(pRepeatButton, 1);

    // Sample rate
    m_pSampleRate = pCallbackControlManager->getControl(
        ConfigKey("[Master]","samplerate"));

    m_pTrackSamples = pCallbackControlManager->addControl(
        new ControlObject(ConfigKey(group, "track_samples")), 1);
    m_pTrackSampleRate = pCallbackControlManager->addControl(
        new ControlObject(ConfigKey(group, "track_samplerate")), 1);

    // Quantization Controller for enabling and disabling the
    // quantization (alignment) of loop in/out positions and (hot)cues with
    // beats.
    addControl(new QuantizeControl(_group, pEngineState));

    // Create the Loop Controller
    m_pLoopingControl = new LoopingControl(_group, pEngineState);
    addControl(m_pLoopingControl);

#ifdef __VINYLCONTROL__
    // If VinylControl is enabled, add a VinylControlControl. This must be done
    // before RateControl is created.
    addControl(new VinylControlControl(group, pEngineState));
#endif

    // Create the Rate Controller
    m_pRateControl = new RateControl(_group, pEngineState);
    addControl(m_pRateControl);

    fwdButton = pCallbackControlManager->getControl(ConfigKey(_group, "fwd"));
    backButton = pCallbackControlManager->getControl(
        ConfigKey(_group, "back"));

    // Create the BPM Controller
    m_pBpmControl = new BpmControl(_group, _config, pEngineState);
    addControl(m_pBpmControl);

    m_pReadAheadManager = new ReadAheadManager(m_pReader);
    m_pReadAheadManager->addEngineControl(m_pLoopingControl);
    m_pReadAheadManager->addEngineControl(m_pRateControl);

    // Construct scaling objects
    m_pScaleLinear = new EngineBufferScaleLinear(m_pReadAheadManager);

    m_pScaleST = new EngineBufferScaleST(m_pReadAheadManager);
    //m_pScaleST = (EngineBufferScaleST*)new EngineBufferScaleDummy(m_pReadAheadManager);
    setPitchIndpTimeStretch(false); // default to VE, let the user specify PITS in their mix

    setNewPlaypos(0.);

    ControlPushButton* pKeylockButton =
            new ControlPushButton(ConfigKey(group, "keylock"));
    pKeylockButton->setButtonMode(ControlPushButton::TOGGLE);
    pKeylockButton->set(false);
    m_pKeylock = pCallbackControlManager->addControl(pKeylockButton, 1);

    //m_iRampIter = 0;
#ifdef __SCALER_DEBUG__
    df.setFileName("mixxx-debug.csv");
    df.open(QIODevice::WriteOnly | QIODevice::Text);
    writer.setDevice(&df);
#endif
}

EngineBuffer::~EngineBuffer()
{
#ifdef __SCALER_DEBUG__
    //close the writer
    df.close();
#endif
    delete m_pReadAheadManager;
    delete m_pReader;

    delete m_pTrackSamples;
    delete m_pTrackSampleRate;

    delete playButton;

    delete rateEngine;
    delete playposSlider;
    delete visualPlaypos;
    delete visualBpm;
    delete m_pKeylock;

    delete m_pRepeat;

    delete m_pScaleLinear;
    delete m_pScaleST;

    delete [] m_pDitherBuffer;

    while (m_engineControls.size() > 0) {
        EngineControl* pControl = m_engineControls.takeLast();
        delete pControl;
    }
}

void EngineBuffer::setPitchIndpTimeStretch(bool b)
{
    ASSERT_ENGINE_THREAD;
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

void EngineBuffer::setNewPlaypos(double newpos) {
    filepos_play = newpos;
    playposSlider->set(filepos_play / file_length_old);

    // Ensures that the playpos slider gets updated in next process call
    m_iSamplesCalculated = 1000000;

    // The right place to do this?
    if (m_pScale)
        m_pScale->clear();
    m_pReadAheadManager->notifySeek(filepos_play);

    for (QList<EngineControl*>::iterator it = m_engineControls.begin();
         it != m_engineControls.end(); it++) {
        EngineControl *pControl = *it;
        pControl->notifySeek(filepos_play);
    }
}

const char * EngineBuffer::getGroup()
{
    return group;
}

double EngineBuffer::getRate()
{
    return m_pRateControl->getRawRate();
}

void EngineBuffer::slotTrackLoaded(TrackPointer pTrack,
                                   int iTrackSampleRate,
                                   int iTrackNumSamples) {
    ASSERT_ENGINE_THREAD;
    //qDebug() << "slotTrackLoaded" << iTrackSampleRate << iTrackNumSamples;
    m_pCurrentTrack = pTrack;
    file_srate_old = iTrackSampleRate;
    file_length_old = iTrackNumSamples;
    m_pTrackSamples->set(iTrackNumSamples);
    m_pTrackSampleRate->set(iTrackSampleRate);
    slotControlSeek(0.);

    // Let the engine know that a track is loaded now.
    m_iTrackLoading = 0;

    // TODO(XXX) replace this with a pipe write to prevent the lock incurred by
    // Qt's event queue.
    emit(trackLoaded(pTrack));
}

// This used to run in the EngineWorker thread pool, but now runs in the
// callback thread.
void EngineBuffer::slotTrackLoadFailed(TrackPointer pTrack,
                                       QString reason) {
    ASSERT_ENGINE_THREAD;
    //qDebug() << "slotTrackLoadFailed" << reason;
    ejectTrack();

    // TODO(XXX) replace this with a pipe write to prevent the lock incurred by
    // Qt's event queue.
    emit(trackLoadFailed(pTrack, reason));
}

TrackPointer EngineBuffer::getLoadedTrack() const {
    return m_pCurrentTrack;
}

void EngineBuffer::ejectTrack() {
    ASSERT_ENGINE_THREAD;

    // Don't allow ejections while playing a track. We don't need to lock to
    // call ControlObject::get() so this is fine.
    if (playButton->get() > 0)
        return;

    TrackPointer pTrack = m_pCurrentTrack;
    m_pCurrentTrack.clear();
    file_srate_old = 0;
    file_length_old = 0;
    playButton->set(0.0);
    visualBpm->set(0.0);
    slotControlSeek(0.);
    m_pTrackSamples->set(0);
    m_pTrackSampleRate->set(0);

    emit(trackUnloaded(pTrack));
}

void EngineBuffer::slotControlSeek(double change) {
    ASSERT_ENGINE_THREAD;
    if (isnan(change)) {
        // This seek is ridiculous.
        return;
    }

    // Find new playpos, restrict to valid ranges.
    double new_playpos = round(change*file_length_old);
    double min_playpos = kMinPlayposRange * file_length_old;
    double max_playpos = kMaxPlayposRange * file_length_old;
    new_playpos = math_max(math_min(new_playpos, max_playpos), min_playpos);

    // TODO(XXX) currently not limiting seeks file_length_old instead of
    // kMaxPlayposRange.
    if (new_playpos > file_length_old)
        new_playpos = file_length_old;

    // Ensure that the file position is even (remember, stereo channel files...)
    if (!even((int)new_playpos)) {
        new_playpos--;
    }

    setNewPlaypos(new_playpos);
}

void EngineBuffer::slotControlSeekAbs(double abs) {
    ASSERT_ENGINE_THREAD;
    slotControlSeek(abs/file_length_old);
}

void EngineBuffer::slotControlPlay(double v)
{
    ASSERT_ENGINE_THREAD;
    // If no track is currently loaded, turn play off.
    if (v > 0.0 && !m_pCurrentTrack) {
        playButton->set(0.0f);
    }
}

void EngineBuffer::slotControlStart(double v)
{
    ASSERT_ENGINE_THREAD;
    if (v > 0.0) {
        slotControlSeek(0.);
    }
}

void EngineBuffer::slotControlEnd(double v)
{
    ASSERT_ENGINE_THREAD;
    if (v > 0.0) {
        slotControlSeek(1.);
    }
}

void EngineBuffer::slotControlPlayFromStart(double v)
{
    ASSERT_ENGINE_THREAD;
    if (v > 0.0) {
        slotControlSeek(0.);
        playButton->set(1);
    }
}

void EngineBuffer::slotControlJumpToStartAndStop(double v)
{
    ASSERT_ENGINE_THREAD;
    if (v > 0.0) {
        slotControlSeek(0.);
        playButton->set(0);
    }
}

void EngineBuffer::slotControlStop(double v)
{
    ASSERT_ENGINE_THREAD;
    if (v > 0.0) {
        playButton->set(0);
    }
}

void EngineBuffer::process(const CSAMPLE *, const CSAMPLE * pOut, const int iBufferSize) {
    g_engineThread = QThread::currentThread();
    Q_ASSERT(even(iBufferSize));
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
    double rate = 0;

    if (m_iTrackLoading == 0) {
        float sr = m_pSampleRate->get();

        double baserate = 0.0f;
        if (sr > 0)
            baserate = ((double)file_srate_old/sr);

        bool paused = playButton->get() != 0.0f ? false : true;

        bool is_scratching = false;
        rate = m_pRateControl->calculateRate(baserate, paused, iBufferSize,
                                             &is_scratching);

        //qDebug() << "rate" << rate << " paused" << paused;

        // Scratching always disables keylock because keylock sounds terrible
        // when not going at a constant rate.
        if (is_scratching && m_pScale != m_pScaleLinear) {
            setPitchIndpTimeStretch(false);
        } else if (!is_scratching) {
            if (m_pKeylock->get() && m_pScale != m_pScaleST) {
                setPitchIndpTimeStretch(true);
            } else if (!m_pKeylock->get() && m_pScale == m_pScaleST) {
                setPitchIndpTimeStretch(false);
            }
        }

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

            // The fileposition should be: (why is this thing a double anyway!?
            // Integer valued.
            double filepos_play_rounded = round(filepos_play);
            if (filepos_play_rounded != filepos_play) {
                qWarning() << __FILE__ << __LINE__ << "ERROR: filepos_play is not round:" << filepos_play;
                filepos_play = filepos_play_rounded;
            }

            // Even.
            if (!even(filepos_play)) {
                qWarning() << "ERROR: filepos_play is not even:" << filepos_play;
                filepos_play--;
            }

            // Perform scaling of Reader buffer into buffer.
            output = m_pScale->scale(0,
                                     iBufferSize,
                                     0,
                                     0);
            double samplesRead = m_pScale->getNewPlaypos();

            // qDebug() << "sourceSamples used " << iSourceSamples
            //          <<" samplesRead " << samplesRead
            //          << ", buffer pos " << iBufferStartSample
            //          << ", play " << filepos_play
            //          << " bufferlen " << iBufferSize;

            // Copy scaled audio into pOutput
            memcpy(pOutput, output, sizeof(pOutput[0]) * iBufferSize);

            // Adjust filepos_play by the amount we processed. TODO(XXX) what
            // happens if samplesRead is a fraction?
            filepos_play =
                    m_pReadAheadManager->getEffectiveVirtualPlaypositionFromLog(
                        static_cast<int>(filepos_play), samplesRead);
        } // else (bCurBufferPaused)

        QListIterator<EngineControl*> it(m_engineControls);
        while (it.hasNext()) {
            EngineControl* pControl = it.next();
            pControl->setCurrentSample(filepos_play, file_length_old);
            double control_seek = pControl->process(rate, filepos_play,
                                                    file_length_old, iBufferSize);

            if (control_seek != kNoTrigger) {
                // If we have not processed loops by this point then we have a
                // bug. RAMAN should be in charge of taking loops now. This
                // final step is more to notify all the EngineControls of the
                // happenings of the engine. TODO(rryan) log condition to a
                // stats-pipe once we have them.

                filepos_play = control_seek;
                double filepos_play_rounded = round(filepos_play);
                if (filepos_play_rounded != filepos_play) {
                    qWarning() << __FILE__ << __LINE__ << "ERROR: filepos_play is not round:" << filepos_play;
                    filepos_play = filepos_play_rounded;
                }

                // Fix filepos_play so that it is not out of bounds.
                if (file_length_old > 0) {
                    if (filepos_play > file_length_old) {
                        // TODO(XXX) limit to kMaxPlayposRange instead of file_length_old
                        filepos_play = file_length_old;
                    } else if(filepos_play < file_length_old * kMinPlayposRange) {
                        filepos_play = kMinPlayposRange * file_length_old;
                    }
                }

                // Safety check that the EngineControl didn't pass us a bogus
                // value
                if (!even(filepos_play))
                    filepos_play--;

                // TODO(XXX) need to re-evaluate this later. If we
                // setNewPlaypos, that clear()'s soundtouch, which might screw
                // up the audio. This sort of jump is a normal event. Also, the
                // EngineControl which caused this jump will get a notifySeek
                // for the same jump which might be confusing. For 1.8.0
                // purposes this works fine. If we do not notifySeek the RAMAN,
                // the engine and RAMAN can get out of sync.

                //setNewPlaypos(filepos_play);
                m_pReadAheadManager->notifySeek(filepos_play);
                // Notify seek the rate control since it needs to track things
                // like looping. Hacky, I know, but this helps prevent things
                // like the scratch controller from flipping out.
                m_pRateControl->notifySeek(filepos_play);
            }
        }


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
    } else { // if (m_iTrackLoading == 0) {
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
    if (m_iRampState == ENGINE_RAMP_UP ||
        m_iRampState == ENGINE_RAMP_DOWN) {
        ramp_inc = m_iRampState * 300 / m_pSampleRate->get();

        for (int i=0; i<iBufferSize; i+=2) {
            if (bCurBufferPaused) {
                float dither = m_pDitherBuffer[m_iDitherBufferReadIndex];
                m_iDitherBufferReadIndex = (m_iDitherBufferReadIndex + 1) % MAX_BUFFER_LEN;
                pOutput[i] = m_fLastSampleValue[0] * m_fRampValue + dither;
                pOutput[i+1] = m_fLastSampleValue[1] * m_fRampValue + dither;
            } else {
                pOutput[i] = pOutput[i] * m_fRampValue;
                pOutput[i+1] = pOutput[i+1] * m_fRampValue;
            }

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
    } else if (m_fRampValue == 0.0) {
        SampleUtil::applyGain(pOutput, 0.0, iBufferSize);
    }

    if ((!bCurBufferPaused && m_iRampState == ENGINE_RAMP_NONE) ||
        (bCurBufferPaused && m_fRampValue == 0.0)) {
        m_fLastSampleValue[0] = pOutput[iBufferSize-2];
        m_fLastSampleValue[1] = pOutput[iBufferSize-1];
    }

#ifdef __SCALER_DEBUG__
    for (int i=0; i<iBufferSize; i+=2) {
        writer << pOutput[i] <<  "\n";
    }
#endif

    m_bLastBufferPaused = bCurBufferPaused;
}

void EngineBuffer::updateIndicators(double rate, int iBufferSize) {
    ASSERT_ENGINE_THREAD;
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

        //Update the BPM even more slowly
        m_iUiSlowTick = (m_iUiSlowTick + 1) % kiBpmUpdateRate;
        if (m_iUiSlowTick == 0) {
            visualBpm->set(m_pBpmControl->getBpm());
        }

        // Reset sample counter
        m_iSamplesCalculated = 0;
    }

    // Update visual control object, this needs to be done more often than the
    // rateEngine and playpos slider
    visualPlaypos->set(fFractionalPlaypos);
}

void EngineBuffer::hintReader(const double dRate,
                              const int iSourceSamples) {
    ASSERT_ENGINE_THREAD;

    m_hintList.clear();
    m_pReadAheadManager->hintReader(dRate, m_hintList, iSourceSamples);

    QListIterator<EngineControl*> it(m_engineControls);
    while (it.hasNext()) {
        EngineControl* pControl = it.next();
        pControl->hintReader(m_hintList);
    }
    m_pReader->hintAndMaybeWake(m_hintList);
}

// WARNING: This method runs in the GUI thread.
// DO NOT TOUCH ANYTHING OTHER THAN m_iTrackLoading and m_pReader.
// Ask rryan before changing this method.
void EngineBuffer::slotLoadTrackFromGuiThread(TrackPointer pTrack) {
    // Raise the track end flag so the EngineBuffer stops processing
    // frames. m_iTrackLoading is a QAtomicInt so this is thread-safe.
    m_iTrackLoading = 1;

    // Even though the track is not yet loaded completely, we load this
    // track. It is safe to do from the GUI thread since assignment of
    // QSharedPointer is thread-safe. If we do not do this here then deadlock
    // will occur since we only process CachingReader messages when
    // EngineBuffer::process is called but EngineBuffer::process is only called
    // if EngineChannel::isActive() is true, which for an EngineDeck is only
    // true if EngineBuffer::isTrackLoaded is true which is only true if
    // m_pCurrentTrack is non-NULL.
    m_pCurrentTrack = pTrack;

    // Signal to the reader to load the track. The reader will respond with
    // either trackLoaded or trackLoadFailed signals.
    m_pReader->newTrack(pTrack);
    m_pReader->wake();
}

void EngineBuffer::addControl(EngineControl* pControl) {
    // Connect to signals from EngineControl here...
    m_engineControls.push_back(pControl);
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
    ASSERT_ENGINE_THREAD;
    pWorkerScheduler->bindWorker(m_pReader);
}

bool EngineBuffer::isTrackLoaded() {
    ASSERT_ENGINE_THREAD;
    if (m_pCurrentTrack) {
        return true;
    }
    return false;
}

void EngineBuffer::slotEjectTrack(double v) {
    ASSERT_ENGINE_THREAD;
    if (v > 0) {
        ejectTrack();
    }
}

void EngineBuffer::setReader(CachingReader* pReader) {
    disconnect(m_pReader, 0, this, 0);
    delete m_pReader;
    m_pReader = pReader;
    m_pReadAheadManager->setReader(pReader);
    connect(m_pReader, SIGNAL(trackLoaded(TrackPointer, int, int)),
            this, SLOT(slotTrackLoaded(TrackPointer, int, int)),
            Qt::DirectConnection);
    connect(m_pReader, SIGNAL(trackLoadFailed(TrackPointer, QString)),
            this, SLOT(slotTrackLoadFailed(TrackPointer, QString)),
            Qt::DirectConnection);
}
