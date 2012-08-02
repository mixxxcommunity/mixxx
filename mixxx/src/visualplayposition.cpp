
#include <qdebug.h>

#include "visualplayposition.h"
#include "controlobjectthreadmain.h"
#include "controlobject.h"



//static
QMap<QString, VisualPlayPosition*> VisualPlayPosition::m_listVisualPlayPosition;

VisualPlayPosition::VisualPlayPosition() :
    m_playPos(-1) {

    m_latency = new ControlObjectThreadMain(
        ControlObject::getControl(ConfigKey("[Master]","latency")));
}

VisualPlayPosition::~VisualPlayPosition() {

}

bool VisualPlayPosition::trySet(double playPos, double rate, double positionStep) {
    if (m_mutex.tryLock()) {
        m_time.start();
        m_playPos = playPos;
        m_rate = rate;
        m_positionStep = positionStep;
        m_mutex.unlock();
        return true;
    }
    return false;
}

double VisualPlayPosition::get() {
    QMutexLocker locker(&m_mutex);

    double playPos;

    if (m_playPos != -1) {
        playPos = m_playPos;
        playPos += m_positionStep * m_time.elapsed() / m_latency->get() * m_rate;
        qDebug() << m_playPos << playPos << m_positionStep << m_time.elapsed()
                 << m_latency->get() << m_rate << QTime::currentTime().msec();
        return playPos;
    }
    return 0;
}

double VisualPlayPosition::getEnginePlayPos() {
    if (m_playPos != -1) {
        return m_playPos;
    } else {
        return 0.0;
    }
}

//static
VisualPlayPosition* VisualPlayPosition::getVisualPlayPosition(QString group) {
    VisualPlayPosition* vpp = m_listVisualPlayPosition[group];
    if (!vpp) {
        vpp = new VisualPlayPosition();
        m_listVisualPlayPosition[group] = vpp;
    }
    return vpp;
}
