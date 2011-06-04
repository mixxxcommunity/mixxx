#include <QtDebug>

#include "playermanager.h"
#include "dlgautodj.h"
#include "library/autodj.h"

AutoDJ::AutoDJ(QObject* parent, PlayerManager* playerManager) :
    QObject(parent),
    m_pPlayerManager(playerManager),
    m_bEnabled(false){

    //connect(DlgAutoDJ, SIGNAL(setAutoDJEnabled(bool)), this, SLOT(setEnabled(bool)));
}


AutoDJ::~AutoDJ(){
}


void AutoDJ::setEnabled(bool enable){

    m_bEnabled = enable;

    if(enable){
        qDebug() << "AutoDJ Enabled";
        //Begin AutoDJ...
    }
}


