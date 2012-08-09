#ifndef VISUALPLAYPOSITION_H
#define VISUALPLAYPOSITION_H

#include <portaudio.h>

#include <QMutex>
#include <QTime>
#include <QMap>

class ControlObjectThreadMain;

class VisualPlayPosition
{
  public:
    VisualPlayPosition();
    ~VisualPlayPosition();

    bool trySet(double playPos, double rate, double positionStep);
    double getAt(const QTime& posTime);
    double getEnginePlayPos();
    static VisualPlayPosition* getVisualPlayPosition(QString group);
    static void setTimeInfo(const PaStreamCallbackTimeInfo *timeInfo);


  private:
    double m_playPos;
    double m_playPosOld;
    double m_rate;
    double m_positionStep;
    QTime m_timeDac;
    int m_deltatime;
    QMutex m_mutex;
    ControlObjectThreadMain* m_latency;
    PaTime m_outputBufferDacTime;

    static QMap<QString, VisualPlayPosition*> m_listVisualPlayPosition;
    static const PaStreamCallbackTimeInfo* m_timeInfo;
    static QTime m_timeInfoTime;

};

#endif // VISUALPLAYPOSITION_H

