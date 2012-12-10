
#include <qdebug.h>

#include "visualplayposition.h"
#include "controlobjectthreadmain.h"
#include "controlobject.h"
#include "waveform/waveformwidgetfactory.h"
#include "mathstuff.h"
#include "waveform/vsyncthread.h"


//static
QMap<QString, VisualPlayPosition*> VisualPlayPosition::m_listVisualPlayPosition;
const PaStreamCallbackTimeInfo* VisualPlayPosition::m_timeInfo;
PerformanceTimer VisualPlayPosition::m_timeInfoTime;

VisualPlayPosition::VisualPlayPosition() :
        m_playPos(-1),
        m_valid(false) {

    m_audioBufferSize = new ControlObjectThreadMain(
        ControlObject::getControl(ConfigKey("[Master]","audio_buffer_size")));
}

VisualPlayPosition::~VisualPlayPosition() {

}

// This function must be called only form the engine thread (PA callback)
bool VisualPlayPosition::trySet(double playPos, double rate,
                double positionStep, double pSlipPosition) {
    if (m_mutex.tryLock()) {
        m_referenceTime = m_timeInfoTime;
        // Time from reference time to Buffer at DAC in µs
        m_timeDac = (m_timeInfo->outputBufferDacTime - m_timeInfo->currentTime) * 1000000;
        m_playPos = playPos;
        m_rate = rate;
        m_positionStep = positionStep;
        m_pSlipPosition = pSlipPosition;
        m_valid = true;
        m_mutex.unlock();
        return true;
    }
    return false;
}

double VisualPlayPosition::getAt(VSyncThread* vsyncThread) {
    QMutexLocker locker(&m_mutex);

    //static double testPos = 0;
    //testPos += 0.000017759; //0.000016608; //  1.46257e-05;
    //return testPos;

    double playPos;

    if (m_valid) {
        int usRefToVSync = vsyncThread->usFromTimerToNextSync(&m_referenceTime);
        int offset = usRefToVSync - m_timeDac;
        playPos = m_playPos;  // load playPos for the first sample in Buffer
        playPos += m_positionStep * offset * m_rate / m_audioBufferSize->get() / 1000;
        //qDebug() << "delta Pos" << playPos - m_playPosOld << offset;
        m_playPosOld = playPos;
        return playPos;
    }
    return -1;
}

void VisualPlayPosition::getPlaySlipAt(int usFromNow, double* playPosition, double* slipPosition) {
    QMutexLocker locker(&m_mutex);

    //static double testPos = 0;
    //testPos += 0.000017759; //0.000016608; //  1.46257e-05;
    //return testPos;

    double playPos;

    if (m_valid) {
        int usElapsed = m_referenceTime.elapsed() / 1000;
        int dacFromNow = usElapsed - m_timeDac;
        int offset = dacFromNow - usFromNow;
        playPos = m_playPos;  // load playPos for the first sample in Buffer
        playPos += m_positionStep * offset * m_rate / m_audioBufferSize->get() / 1000;
        *playPosition = playPos;
    } else {
        *playPosition = -1;
    }
    *slipPosition = m_pSlipPosition;
}

double VisualPlayPosition::getEnginePlayPos() {
    if (m_valid) {
        return m_playPos;
    } else {
        return -1;
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
    m_timeInfoTime.start();
    //qDebug() << "TimeInfo" << (timeInfo->currentTime - floor(timeInfo->currentTime)) << (timeInfo->outputBufferDacTime - floor(timeInfo->outputBufferDacTime));
    //m_timeInfo.currentTime = timeInfo->currentTime;
    //m_timeInfo.inputBufferAdcTime = timeInfo->inputBufferAdcTime;
    //m_timeInfo.outputBufferDacTime = timeInfo->outputBufferDacTime;
}

