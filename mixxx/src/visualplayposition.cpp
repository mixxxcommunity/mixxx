
#include <qdebug.h>

#include "visualplayposition.h"
#include "controlobjectthreadmain.h"
#include "controlobject.h"
#include "waveform/waveformwidgetfactory.h"
#include "mathstuff.h"



//static
QMap<QString, VisualPlayPosition*> VisualPlayPosition::m_listVisualPlayPosition;
const PaStreamCallbackTimeInfo* VisualPlayPosition::m_timeInfo;
QTime VisualPlayPosition::m_timeInfoTime;

VisualPlayPosition::VisualPlayPosition() :
    m_playPos(-1) {

    m_audioBufferSize = new ControlObjectThreadMain(
        ControlObject::getControl(ConfigKey("[Master]","audio_buffer_size")));
}

VisualPlayPosition::~VisualPlayPosition() {

}

// This function must be called only form the engine thread (PA callback)
bool VisualPlayPosition::trySet(double playPos, double rate, double positionStep) {
    if (m_mutex.tryLock()) {
        //QTime now = QTime::currentTime();
        //qDebug() << now.msec();
        //m_deltatime = m_time.msecsTo(now);
        //m_outputBufferDacTime = m_timeInfo->outputBufferDacTime;

        PaTime latencyCorrection = m_timeInfo->outputBufferDacTime - m_timeInfo->currentTime;
        m_timeDac = m_timeInfoTime.addMSecs(latencyCorrection * 1000);
        m_playPos = playPos;
        m_rate = rate;
        m_positionStep = positionStep;
        m_mutex.unlock();
        return true;
    }
    return false;
}

double VisualPlayPosition::getAt(const QTime& posTime) {
    QMutexLocker locker(&m_mutex);

    static double testPos = 0;
    testPos += 0.000016608; //  1.46257e-05;
    return testPos;

    double playPos;

    if (m_playPos != -1) {
        int msecsToPosTime = m_timeDac.msecsTo(posTime);
        playPos = m_playPos;  // load playPos for the first sample in Buffer
        playPos += m_positionStep * msecsToPosTime / m_audioBufferSize->get() * m_rate;
        //qDebug() << "delta Pos" << playPos - m_playPosOld << msecsToPosTime;
        m_playPosOld = playPos;
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

//static
void VisualPlayPosition::setTimeInfo(const PaStreamCallbackTimeInfo *timeInfo) {
    m_timeInfo = timeInfo;
    m_timeInfoTime = QTime::currentTime();
    //qDebug() << "TimeInfo" << (timeInfo->currentTime - floor(timeInfo->currentTime)) << (timeInfo->outputBufferDacTime - floor(timeInfo->outputBufferDacTime));
    //m_timeInfo.currentTime = timeInfo->currentTime;
    //m_timeInfo.inputBufferAdcTime = timeInfo->inputBufferAdcTime;
    //m_timeInfo.outputBufferDacTime = timeInfo->outputBufferDacTime;
}

