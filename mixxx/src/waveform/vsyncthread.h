#ifndef VSYNCTHREAD_H
#define VSYNCTHREAD_H

#include <QTime>
#include <QThread>
#include <QSemaphore>

#include <qx11info_x11.h>
#include "performancetimer.h"


#if defined(__APPLE__)

#elif defined(__WINDOWS__)

#else
    #include <GL/glx.h>
    #include "GL/glxext.h"
#endif


class QGLWidget;

class VSyncThread : public QThread {
    Q_OBJECT
  public:
    VSyncThread(QWidget* parent);
    ~VSyncThread();
    void run();
    void stop();

    bool waitForVideoSync(QGLWidget* glw);
    int elapsed();
    int usToNextSync();
    void setUsSyncTime(int usSyncTimer);
    void setVSync(bool checked);
    int rtErrorCnt();
    void setSwapWait(int sw);
    int usFromTimerToNextSync(PerformanceTimer* timer);
    void vsyncSlotFinished();


  signals:
    void vsync1();
    void vsync2();
        
  private:
    bool doRendering;
    QGLWidget *m_glw;

#if defined(__APPLE__)

#elif defined(__WINDOWS__)

#else
    void initGlxext(QGLWidget* glw);
    bool glXExtensionSupported(Display *dpy, int screen, const char *extension);

    PFNGLXGETVIDEOSYNCSGIPROC glXGetVideoSyncSGI;
    PFNGLXWAITVIDEOSYNCSGIPROC glXWaitVideoSyncSGI;
    PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI;
    PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT;

    PFNGLXGETSYNCVALUESOMLPROC glXGetSyncValuesOML;
    PFNGLXGETMSCRATEOMLPROC glXGetMscRateOML;
    PFNGLXSWAPBUFFERSMSCOMLPROC glXSwapBuffersMscOML;
    PFNGLXWAITFORMSCOMLPROC glXWaitForMscOML;
    PFNGLXWAITFORSBCOMLPROC  glXWaitForSbcOML;

    uint m_counter;
    bool m_firstRun;
#endif

    int m_usSyncTime;
    int m_usWait;
    bool m_vSync;
    bool m_syncOk;
    int m_rtErrorCnt;
    int m_swapWait;
    PerformanceTimer m_timer;
    QSemaphore m_sema;
};


#endif // VSYNCTHREAD_H
