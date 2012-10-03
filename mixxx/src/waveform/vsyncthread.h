#ifndef VSYNCTHREAD_H
#define VSYNCTHREAD_H

#include <QTime>
#include <QThread>

#include <qx11info_x11.h>
#include "performancetimer.h"

class QGLWidget;

class VSyncThread : public QThread {
    Q_OBJECT
  public:
    VSyncThread(QWidget* parent);
    ~VSyncThread();
    void run();
    void stop();

    bool waitForVideoSync();
    qint64 elapsed();
    void setSyncTime(int syncTimer);

  signals:
    void vsync();
        
  private:
    bool doRendering;
    QGLWidget *glw;

#if defined(__APPLE__)

#elif defined(__WINDOWS__)

#else
    bool glXExtensionSupported(Display *dpy, int screen, const char *extension);

    typedef int (*qt_glXGetVideoSyncSGI)(uint *);
    typedef int (*qt_glXWaitVideoSyncSGI)(int, int, uint *);
    typedef int (*qt_glXSwapIntervalSGI)(int interval);

    qt_glXGetVideoSyncSGI glXGetVideoSyncSGI;
    qt_glXWaitVideoSyncSGI glXWaitVideoSyncSGI;
    qt_glXSwapIntervalSGI glXSwapIntervalSGI;

    uint m_counter;
    int m_syncTime;
#endif

    PerformanceTimer m_timer;
};


#endif // VSYNCTHREAD_H
