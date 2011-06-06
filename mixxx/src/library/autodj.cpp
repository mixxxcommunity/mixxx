#include <QtDebug>

#include "playermanager.h"
#include "deck.h"
#include "library/autodj.h"

AutoDJ::AutoDJ(QObject* parent, DlgAutoDJ* dlgAutoDJ, PlayerManager* playerManager) :
    QObject(parent),
    m_pPlayerManager(playerManager),
    m_pDlgAutoDJ(dlgAutoDJ),
    m_bEnabled(false){

    connect(dlgAutoDJ, SIGNAL(setAutoDJEnabled(bool)),
            this, SLOT(setEnabled(bool)));

    connect(dlgAutoDJ, SIGNAL(sendNextTrack(TrackPointer)),
            this, SLOT(receiveNextTrack(TrackPointer)));
}


AutoDJ::~AutoDJ(){
}


void AutoDJ::setEnabled(bool enable){

    m_bEnabled = enable;

    if(enable){
        qDebug() << "AutoDJ Enabled";
        // Begin AutoDJ...
        refreshPlayerStates();
    }
}

void AutoDJ::refreshPlayerStates(){

    // Loop through all decks, check which ones are playing
    for (int i = 1; i < m_pPlayerManager->numDecks(); i++){
        Deck* deck = m_pPlayerManager->getDeck(i);
        // deck->check if already playing
    }


}

void AutoDJ::receiveNextTrack(TrackPointer nextTrack){

}


