#ifndef VSYNCTHREAD_H
#define VSYNCTHREAD_H

#include <QTime>
#include <QThread>

class QGLWidget;

class VSyncThread : public QThread {
    Q_OBJECT
  public:
    VSyncThread();
    ~VSyncThread();
    void run();
    void stop();

  signals:
    void vsync();
        
  private:
    bool doRendering;
    QGLWidget *glw;
};


#endif // VSYNCTHREAD_H
