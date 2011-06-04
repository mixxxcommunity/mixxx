#include <QtDebug>

#include "playermanager.h"
#include "library/autodj.h"

AutoDJ::AutoDJ(QObject* parent, PlayerManager* playerManager) :
    QObject(parent)
{
}


AutoDJ::~AutoDJ()
{
}

void AutoDJ::setEnabled(bool enable){

    m_bEnabled = enable;

    if(enable){
        qDebug() << "AutoDJ enabled";
    }
}


