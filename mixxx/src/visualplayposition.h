#ifndef VISUALPLAYPOSITION_H
#define VISUALPLAYPOSITION_H

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
    double get();
    double getEnginePlayPos();
    static VisualPlayPosition* getVisualPlayPosition(QString group);


  private:
    double m_playPos;
    double m_rate;
    double m_positionStep;
    QTime m_time;
    QMutex m_mutex;
    ControlObjectThreadMain* m_latency;

    static QMap<QString, VisualPlayPosition*> m_listVisualPlayPosition;

};

#endif // VISUALPLAYPOSITION_H

