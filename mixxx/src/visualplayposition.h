#ifndef VISUALPLAYPOSITION_H
#define VISUALPLAYPOSITION_H

#include <portaudio.h>
#include "util/performancetimer.h"

#include <QMutex>
#include <QTime>
#include <QMap>

class ControlObjectThreadMain;
class VSyncThread;

class VisualPlayPosition
{
  public:
    VisualPlayPosition();
    ~VisualPlayPosition();

    bool trySet(double playPos, double rate, double positionStep, double pSlipPosition);
    double getAt(VSyncThread* vsyncThread);
    void getPlaySlipAt(int usFromNow, double* playPosition, double* slipPosition);
    double getEnginePlayPos();
    static VisualPlayPosition* getVisualPlayPosition(QString group);
    static void setTimeInfo(const PaStreamCallbackTimeInfo *timeInfo);
    void setInvalid() { m_valid = false; };


  private:
    bool m_valid;
    double m_playPos;
    double m_playPosOld;
    double m_rate;
    double m_positionStep;
    double m_pSlipPosition;
    int m_timeDac;
    PerformanceTimer m_referenceTime;
    int m_deltatime;
    QMutex m_mutex;
    ControlObjectThreadMain* m_audioBufferSize;
    PaTime m_outputBufferDacTime;

    static QMap<QString, VisualPlayPosition*> m_listVisualPlayPosition;
    static const PaStreamCallbackTimeInfo* m_timeInfo;
    static PerformanceTimer m_timeInfoTime;

};

#endif // VISUALPLAYPOSITION_H

