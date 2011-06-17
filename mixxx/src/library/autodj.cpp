#include <QtDebug>

#include "playermanager.h"
#include "deck.h"
#include "controlobject.h"
#include "controlobjectthreadmain.h"
#include "library/autodj.h"

AutoDJ::AutoDJ(QObject* parent) :
    QObject(parent),
    //m_pPlayerManager(playerManager),
    m_bEnabled(false){

    m_bPlayer1Primed = false;
    m_bPlayer2Primed = false;

    connect(parent, SIGNAL(sendNextTrack(TrackPointer)),
            this, SLOT(receiveNextTrack(TrackPointer)));

    m_pCOPlayPos1 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel1]", "playposition")));
    m_pCOPlayPos2 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel2]", "playposition")));
    m_pCOPlay1 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel1]", "play")));
    m_pCOPlay2 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel2]", "play")));
    m_pCORepeat1 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel1]", "repeat")));
    m_pCORepeat2 = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Channel2]", "repeat")));
    m_pCOCrossfader = new ControlObjectThreadMain(
                            ControlObject::getControl(ConfigKey("[Master]", "crossfader")));
}


AutoDJ::~AutoDJ(){
    delete m_pCOPlayPos1;
    delete m_pCOPlayPos2;
    delete m_pCOPlay1;
    delete m_pCOPlay2;
    delete m_pCORepeat2;
    delete m_pCOCrossfader;
}


void AutoDJ::setEnabled(bool enable){

    if(enable){
        qDebug() << "AutoDJ Enabled";
        // Begin AutoDJ...
        if (m_pCOPlay1->get() == 1.0f && m_pCOPlay2->get() == 1.0f) {
            qDebug() << "One player must be stopped before enabling Auto DJ mode";
            //pushButtonAutoDJ->setChecked(false);
            return;
        }

        m_bEnabled = enable;
        connect(m_pCOPlayPos1, SIGNAL(valueChanged(double)),
        this, SLOT(player1PositionChanged(double)));
        connect(m_pCOPlayPos2, SIGNAL(valueChanged(double)),
        this, SLOT(player2PositionChanged(double)));

        //Manually override the "next track is already loaded" flag
        //because we've already primed a player with the first track.
        //We do this so that you don't lose the first song in your
        //Auto DJ queue if you enable Auto DJ then change your mind
        //and disable it right away. This just makes it a little bit
        //more user friendly. :)
        //m_bNextTrackAlreadyLoaded = true;
        m_bPlayer1Primed = false;
        m_bPlayer2Primed = false;

        //If there are no tracks in the Auto DJ queue, disable Auto DJ mode.
       /* if (m_pAutoDJTableModel->rowCount() == 0)
        {
            //Queue was empty. Disable and return.
            pushButtonAutoDJ->setChecked(false);
            return;
        }*/ //don't need this code, above block takes care of this case.

        //If only one of the players is playing...
        if ((m_pCOPlay1->get() == 1.0f && m_pCOPlay2->get() == 0.0f) ||
            (m_pCOPlay1->get() == 0.0f && m_pCOPlay2->get() == 1.0f))
        {
            //Load the first song from the queue.
            if (!loadNextTrackFromQueue(false)) {
                //Queue was empty. Disable and return.
                //pushButtonAutoDJ->setChecked(false);
                return;
            }
            //Set the primed flags so the crossfading algorithm knows
            //that it doesn't need to load a track into whatever player.
            if (m_pCOPlay1->get() == 1.0f)
            {
                m_bPlayer1Primed = true;
            }
            if (m_pCOPlay2->get() == 1.0f)
            {
                m_bPlayer2Primed = true;
            }
        }

        //If both players are stopped, start the first one (which should have just had a track loaded into it)
        else if (m_pCOPlay1->get() == 0.0f && m_pCOPlay2->get() == 0.0f) {
            //Load the first song from the queue.
            if (!loadNextTrackFromQueue(false)) {
                //Queue was empty. Disable and return.
                //pushButtonAutoDJ->setChecked(false);
                return;
            }
            m_pCOCrossfader->slotSet(-1.0f); //Move crossfader to the left!
            m_pCORepeat1->slotSet(1.0f); //Turn on repeat mode to avoid race condition between async load
                                               //and "play" command.
            m_pCOPlay1->slotSet(1.0f); //Play the track in player 1
        }
    }
    else { //Disable AutoDJ
        //pushButtonAutoDJ->setText(tr("Enable Auto DJ"));
        qDebug() << "Auto DJ disabled";
        m_bEnabled = false;
        m_pCOPlayPos1->disconnect(this);
        m_pCOPlayPos2->disconnect(this);
        m_pCORepeat1->slotSet(0.0f); //Turn off repeat mode
        m_pCORepeat2->slotSet(0.0f); //Turn off repeat mode
    }
}

void AutoDJ::player1PositionChanged(double value)
{
    const float posThreshold = 0.95; //95% playback is when we crossfade and do stuff
    if (value > posThreshold)
    {
        //Crossfade!
        float crossfadeValue = -1.0f + 2*(value-posThreshold)/(1.0f-posThreshold);
        m_pCOCrossfader->slotSet(crossfadeValue); //Move crossfader to the right!
        //If the second player doesn't have a new track loaded in it...
        if (!m_bPlayer2Primed)
        {
            qDebug() << "pp1c loading";

            //Load the next track into Player 2
            //if (!m_bNextTrackAlreadyLoaded) //Fudge to make us not skip the first track
            {
                if (!loadNextTrackFromQueue(true))
                    return;
            }
            //m_bNextTrackAlreadyLoaded = false; //Reset fudge
            m_bPlayer2Primed = true;
        }
        //If the second player is stopped...
        if (m_pCOPlay2->get() == 0.0f)
        {
            //Turn off repeat mode to tell Player 1 to stop at the end
            m_pCORepeat1->slotSet(0.0f);

            //Turn on repeat mode to tell Player 2 to start playing when the new track is loaded.
            //This helps us get around the fact that it takes time for the track to be loaded
            //and that is executed asynchronously (so we get around the race condition).
            m_pCORepeat2->slotSet(1.0f);
            //Play!
            m_pCOPlay2->slotSet(1.0f);
        }

        if (value == 1.0f)
        {
            m_pCOPlay1->slotSet(0.0f); //Stop the player
            m_bPlayer1Primed = false;
        }
    }
}

void AutoDJ::player2PositionChanged(double value)
{
    const float posThreshold = 0.95; //95% playback is when we crossfade and do stuff
    if (value > posThreshold)
    {
        //Crossfade!
        float crossfadeValue = 1.0f - 2*(value-posThreshold)/(1.0f-posThreshold);
        m_pCOCrossfader->slotSet(crossfadeValue); //Move crossfader to the right!

        //If the first player doesn't have the next track loaded, load a track into
        //it and start playing it!
        if (!m_bPlayer1Primed)
        {
            //Load the next track into player 1
            //if (!m_bNextTrackAlreadyLoaded) //Fudge to make us not skip the first track
            {
                if (!loadNextTrackFromQueue(true))
                    return;
            }
            //m_bNextTrackAlreadyLoaded = false; //Reset fudge
            m_bPlayer1Primed = true;
        }
        if (m_pCOPlay1->get() == 0.0f)
        {
            //Turn off repeat mode to tell Player 2 to stop at the end
            m_pCORepeat2->slotSet(0.0f);

            //Turn on repeat mode to tell Player 1 to start playing when the new track is loaded.
            //This helps us get around the fact that it takes time for the track to be loaded
            //and that is executed asynchronously (so we get around the race condition).
            m_pCORepeat1->slotSet(1.0f);
            m_pCOPlay1->slotSet(1.0f);
        }

        if (value == 1.0f)
        {
            m_pCOPlay2->slotSet(0.0f); //Stop the player
            m_bPlayer2Primed = false;
        }
    }
}

void AutoDJ::refreshPlayerStates(){

    // Loop through all decks, check which ones are playing
    //for (int i = 1; i < m_pPlayerManager->numDecks(); i++){
    //    Deck* deck = m_pPlayerManager->getDeck(i);
        // deck->check if already playing
    //}
}

void AutoDJ::receiveNextTrack(TrackPointer nextTrack){
    qDebug() << "Received track!" << nextTrack;

}


