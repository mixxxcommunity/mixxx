#include <QtDebug>

#include "controlobject.h"
#include "controlobjectthreadmain.h"
#include "playerinfo.h"
#include "library/autodj.h"
#include "library/tracktransition.h"

AutoDJ::AutoDJ(QObject* parent, ConfigObject<ConfigValue>* pConfig) :
    QObject(parent),
    m_bEnabled(false),
    m_pConfig(pConfig) {

    m_bPlayer1Primed = false;
    m_bPlayer2Primed = false;
    m_bPlayer1Cued = false;
    m_bPlayer2Cued = false;
    m_bEndOfPlaylist = false;


    m_pTrackTransition = new TrackTransition(this, m_pConfig);

    m_pCOPlay1 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel1]", "play")));
    m_pCOPlay2 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel2]", "play")));
    m_pCORepeat1 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel1]", "repeat")));
    m_pCORepeat2 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel2]", "repeat")));
    m_pCOPlayPosSamples1 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel1]", "playposition_samples")));
    m_pCOPlayPosSamples2 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel2]", "playposition_samples")));
    m_pCOCrossfader = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Master]", "crossfader")));
    // TODO(tom__m) These two COs are only used to check if at the end of the track... find a better way to do this
    m_pCOPlayPos1 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel1]", "playposition")));
    m_pCOPlayPos2 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel2]", "playposition")));
    m_pCOSampleRate1 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel1]", "track_samplerate")));
    m_pCOSampleRate2 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel2]", "track_samplerate")));
}


AutoDJ::~AutoDJ() {
    delete m_pCOPlayPosSamples1;
    delete m_pCOPlayPosSamples2;
    delete m_pCOPlay1;
    delete m_pCOPlay2;
    delete m_pCORepeat1;
    delete m_pCORepeat2;
    delete m_pCOCrossfader;
}


void AutoDJ::setEnabled(bool enable) {

    // Check if we have yet to receive a track to load
    if(m_pNextTrack == NULL) {
        emit needNextTrack();
    }

    if(enable) {
        qDebug() << "AutoDJ Enabled";
        // Begin AutoDJ...
        if (m_pCOPlay1->get() == 1.0f && m_pCOPlay2->get() == 1.0f) {
            qDebug() << "One player must be stopped before enabling Auto DJ mode";
            emit disableAutoDJ();
            return;
        }

        m_bEnabled = enable;

        // Right now we are just using deck 1 and 2, in the future this will be
        // determined by which decks the user wants to use for AutoDJ
        connect(m_pCOPlayPosSamples1, SIGNAL(valueChanged(double)),
        this, SLOT(player1PositionChanged(double)));
        connect(m_pCOPlayPosSamples2, SIGNAL(valueChanged(double)),
        this, SLOT(player2PositionChanged(double)));

        m_bPlayer1Primed = false;
        m_bPlayer2Primed = false;
        m_bPlayer1Cued = false;
        m_bPlayer2Cued = false;

        // If only one of the players is playing...
        if ((m_pCOPlay1->get() == 1.0f && m_pCOPlay2->get() == 0.0f) ||
            (m_pCOPlay1->get() == 0.0f && m_pCOPlay2->get() == 1.0f)) {

            // Load the first song from the queue.
            if (m_bEndOfPlaylist) {
                // Queue is empty. Disable and return.
                emit disableAutoDJ();
                return;
            }
            loadNextTrack();

            // Set the primed flags so the crossfading algorithm knows
            // that it doesn't need to load a track into whatever player.
            if (m_pCOPlay1->get() == 1.0f)
            {
                m_bPlayer1Primed = true;
            }
            if (m_pCOPlay2->get() == 1.0f)
            {
                m_bPlayer2Primed = true;
            }
        }

        // If both players are stopped, start the first one
        else if (m_pCOPlay1->get() == 0.0f && m_pCOPlay2->get() == 0.0f) {
            // Load the first song from the queue.
            if (m_bEndOfPlaylist) {
                // Queue was empty. Disable and return.
                emit disableAutoDJ();
                return;
            }
            loadNextTrack();

            m_pCORepeat1->slotSet(1.0f); // Turn on repeat mode to avoid race condition between async load
            m_pCOPlay1->slotSet(1.0f);  // Play the track in player 1
            m_pCOCrossfader->slotSet(-1.0f);
        }
    }
    else {
        // Disable AutoDJ
        emit disableAutoDJ();
        qDebug() << "Auto DJ disabled";
        m_bEnabled = false;
        m_pCOPlayPosSamples1->disconnect(this);
        m_pCOPlayPosSamples2->disconnect(this);
        m_pCORepeat1->slotSet(0.0f); //Turn off repeat mode
        m_pCORepeat2->slotSet(0.0f); //Turn off repeat mode
    }
}

void AutoDJ::player1PositionChanged(double value) {

    TrackPointer pTrack = PlayerInfo::Instance().getTrackInfo("[Channel1]");

    // Check if the other player already has a track loaded
    if(!m_bPlayer2Primed) {
        // Load the next track into the other player
        if (!m_bEndOfPlaylist) {
            loadNextTrack();
            m_bPlayer2Primed = true;
        }
    }

    // Check if Player 1 position is past the Fade Out point
    if (m_pCOPlayPosSamples1->get() > pTrack->getFadeOut()) {

        m_pTrackTransition->transition("[Channel1]", "[Channel2]");

        // Cue the track in Player 2 to the Fade In point
        if (m_bPlayer2Cued == false) {
            cueTrackToFadeIn("[Channel2]");
            m_bPlayer2Cued = true;
        }

        // Check if other player is stopped
        if (m_pCOPlay2->get() == 0.0f) {
            // Turn off repeat mode in Player 1
            m_pCORepeat1->slotSet(0.0f);

            // Turn on repeat mode so Player 2 will start playing when the new track is loaded.
            // This helps us get around the fact that it takes time for the track to be loaded
            // and that is executed asynchronously (this gets around the race condition).
            m_pCORepeat2->slotSet(1.0f);
            // Play Player 2
            m_pCOPlay2->slotSet(1.0f);
        }

        if (m_pCOPlayPos1->get() == 1.0f) {
            m_pCOPlay1->slotSet(0.0f);
            m_bPlayer1Primed = false;
        }
    }
}

void AutoDJ::player2PositionChanged(double value) {

    TrackPointer pTrack = PlayerInfo::Instance().getTrackInfo("[Channel2]");

    // Check if Player 2 position is past the Fade Out point
    if (m_pCOPlayPosSamples2->get() > pTrack->getFadeOut()) {

        m_pTrackTransition->transition("[Channel2]", "[Channel1]");

        // Cue the track in Player 1 to the Fade In point
        if (m_bPlayer1Cued == false) {
            cueTrackToFadeIn("[Channel1]");
            m_bPlayer1Cued = true;
        }

        // Check if the other player already has a track loaded
        if (!m_bPlayer1Primed) {
            // Load the next track into the other player
            if (!m_bEndOfPlaylist) {
                loadNextTrack();
                m_bPlayer1Primed = true;
            }
        }

        // Check if other player is stopped
        if (m_pCOPlay1->get() == 0.0f) {
            // Turn off repeat mode in Player 2
            m_pCORepeat2->slotSet(0.0f);

            // Turn on repeat mode so Player 1 will start playing when the new track is loaded.
            // This helps us get around the fact that it takes time for the track to be loaded
            // and that is executed asynchronously (this gets around the race condition).
            m_pCORepeat1->slotSet(1.0f);
            // Play Player 1
            m_pCOPlay1->slotSet(1.0f);
        }

        if (m_pCOPlayPos2->get() == 1.0f) {
            m_pCOPlay2->slotSet(0.0f);
            m_bPlayer2Primed = false;
        }
    }
}

void AutoDJ::receiveNextTrack(TrackPointer nextTrack) {
    m_pNextTrack = nextTrack;
}

void AutoDJ::setEndOfPlaylist(bool endOfPlaylist) {
    m_bEndOfPlaylist = endOfPlaylist;
}

void AutoDJ::loadNextTrack() {
    emit loadTrack(m_pNextTrack);
    emit needNextTrack();
}

void AutoDJ::cueTrackToFadeIn(QString group) {

    TrackPointer pTrack = PlayerInfo::Instance().getTrackInfo(group);

    if (group == "[Channel1]") {
    }

    if (group == "[Channel2]") {
        // Get the FadeIn point of the track (in samples)
        double fadeIn = pTrack->getFadeIn();
        qDebug() << fadeIn;

        // Divide this by 2 and then by the sample rate to convert it to seconds
        fadeIn = (fadeIn/2)/(m_pCOSampleRate2->get());
        qDebug() << fadeIn;

        // Subtract the crossfade length
        fadeIn = fadeIn - (m_pTrackTransition->m_iFadeLength);
        qDebug() << fadeIn;

        // Check if the crossfade length set our cue to negative
        if (fadeIn < 0) {
            fadeIn = 0;
        }
        // Convert fadeIn to a ratio of the whole track
        fadeIn = fadeIn/(pTrack->getDuration());
        qDebug() << pTrack->getDuration();
        // Cue up the fadeIn point
        qDebug() << fadeIn;
        m_pCOPlayPos2->slotSet(fadeIn);
    }
}


