#ifndef VSYNCTHREAD_H
#define VSYNCTHREAD_H

#include <QTime>
#include <QThread>

#include <qx11info_x11.h>

class QGLWidget;

class VSyncThread : public QThread {
    Q_OBJECT
  public:
    VSyncThread(QWidget* parent);
    ~VSyncThread();
    void run();
    void stop();

    bool waitForVideoSync();


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

};


#endif // VSYNCTHREAD_H
