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
    int elapsed();
    int usToNextSync();
    void setUsSyncTime(int usSyncTimer);
    void setVSync(bool checked);
    int rtErrorCnt();

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
#endif

    int m_usSyncTime;
    int m_usWait;
    bool m_vSync;
    int m_rtErrorCnt;
    PerformanceTimer m_timer;
};


#endif // VSYNCTHREAD_H
