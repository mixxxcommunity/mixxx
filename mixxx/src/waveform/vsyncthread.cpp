
#include <QThread>
#include <QGLWidget>
#include <QGLFormat>
#include <QTime>
#include <qdebug.h>
#include <QTime>
#if defined(__APPLE__)

#elif defined(__WINDOWS__)

#else
   #include <GL/glx.h>
#endif


#include "vsyncthread.h"
#include "performancetimer.h"

#if defined(__APPLE__)

#elif defined(__WINDOWS__)

#else
   extern const QX11Info *qt_x11Info(const QPaintDevice *pd);
#endif

VSyncThread::VSyncThread(QWidget* parent)
        : QThread(),
          m_usSyncTime(33333),
          m_vSync(false),
          m_rtErrorCnt(0),
          m_swapWait(0) {
    doRendering = true;

    //QGLFormat glFormat = QGLFormat::defaultFormat();
    //glFormat.setSwapInterval(1);
    //glw = new QGLWidget(glFormat);

    glw = new QGLWidget(parent);
    glw->resize(1,1);
    /*
    qDebug() << parent->size();
    glw->resize(parent->size());
    glw->hide();
    qDebug() << glw->size();
    qDebug() << glw->pos();
    qDebug() << glw->mapToGlobal(QPoint(0,0));
    */

#if defined(__APPLE__)

#elif defined(__WINDOWS__)

#else
    const QX11Info *xinfo = qt_x11Info(glw);
    if (glXExtensionSupported(xinfo->display(), xinfo->screen(), "GLX_SGI_video_sync")) {
        glXGetVideoSyncSGI =  (qt_glXGetVideoSyncSGI)glXGetProcAddressARB((const GLubyte *)"glXGetVideoSyncSGI");
        glXWaitVideoSyncSGI = (qt_glXWaitVideoSyncSGI)glXGetProcAddressARB((const GLubyte *)"glXWaitVideoSyncSGI");
    }

    if (glXExtensionSupported(xinfo->display(), xinfo->screen(), "GLX_SGI_swap_control")) {
        glXSwapIntervalSGI =  (qt_glXSwapIntervalSGI)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
    }
#endif
}

VSyncThread::~VSyncThread() {
    doRendering = false;
    wait();
    delete glw;
}

void VSyncThread::stop()
{
    doRendering = false;
}


void VSyncThread::run() {
    QThread::currentThread()->setObjectName("VSyncThread");

    glw->makeCurrent();

    int usRest;
    int usLast;

    bool inSync;

    m_usWait = m_usSyncTime;
    m_timer.start();

    while (doRendering) {
        inSync = false;
        if (m_vSync) {
            usRest = m_usWait - m_timer.elapsed() / 1000;
            if (usRest > 100) {
                inSync = waitForVideoSync();
            }
        }

        if (!inSync) {
            // sync by sleep
            usRest = m_usWait - m_timer.elapsed() / 1000;
            if (usRest > 100) {
                usleep(usRest);
            }
        }
        // <- Assume we are VSynced here ->
        // now we have one VSync interval time for swap.
        usLast = m_timer.restart() / 1000;
        if (usRest < 0) {
            // Swaping was delayed
            // Asume the real swap happens on the following VSync
            m_rtErrorCnt++; // Count as Real Time Error
        }
        usRest = m_usWait - usLast;
        if (usRest < -8333) { // -8.333 ms for up to 120 Hz Displays
            // Out of syc, start new
            m_usWait = m_usSyncTime;
        } else if (!inSync) {
            // try to stay in right intervals
            m_usWait = m_usSyncTime + (usRest % m_usSyncTime);
        } else {
            // we are synced
            m_usWait = m_usSyncTime;
        }
        emit(vsync1()); // renders the waveform, Possible delayed due to anti tearing
        m_usWait += m_swapWait; // shift interval to avoid waiting for swap again

        // Sleep to hit the desired interval and avoid a jitter
        // by swapping to early when using a frame rate smaler then the display refresh rate
        usRest = m_usWait - m_timer.elapsed() / 1000;
        usRest -= 8333;  // -8.333 ms for up to 120 Hz Displays
        if (usRest > 100) {
            usleep(usRest);
        }
        emit(vsync2()); // swaps the new waveform to front


        // qDebug()  << "VSync 4                           " << usLast << inSync;
    }
}

bool VSyncThread::waitForVideoSync() {
    uint counter;
    //uint counter_start;

    if (!glw->parentWidget()->isVisible()) {
        return false;
    }

    if (QGLContext::currentContext() != glw->context()) {
        glw->makeCurrent();
    }

#if defined(__APPLE__)

#elif defined(__WINDOWS__)

#else
    if (glXGetVideoSyncSGI && glXWaitVideoSyncSGI) {
        if (!glXWaitVideoSyncSGI(1, 0, &counter)) {
            return true;
        }
 /*
        if (!glXGetVideoSyncSGI(&counter_start)) {
            counter = counter_start;
            glXWaitVideoSyncSGI(2, (counter + 1) % 2, &counter);
            if (counter < counter_start)
                fprintf(stderr, "error:  vblank count went backwards: %d -> %d\n", counter_start, counter);
            if (counter == counter_start)
                fprintf(stderr, "error:  count didn't change: %d\n", counter);
            if (counter > counter_start + 1)
                fprintf(stderr, "error:  one counter lost: %d\n", counter);
            return true;
        }
*/
        //    qDebug() << m_counter;
        // glXWaitVideoSyncSGI(2, (m_counter + 1) % 2, &m_counter);
        //glXWaitVideoSyncSGI(1, 0, &m_counter);
            //qDebug() << m_counter;
        //    glXWaitVideoSyncSGI(1, 0, &m_counter);
        //    qDebug() << "waitForVideoSync()" << m_counter;
        //}
        // https://code.launchpad.net/~vanvugt/compiz/fix-763005-trunk/+merge/71307
        // http://www.bitsphere.co.za/gameDev/openGL/vsync.php
        // http://www.inb.uni-luebeck.de/~boehme/xvideo_sync.html
    }
#endif
    return false;
}


#if defined(__APPLE__)

#elif defined(__WINDOWS__)

#else
// from mesa-demos-8.0.1/src/xdemos/glsync.c (MIT license)
bool VSyncThread::glXExtensionSupported(Display *dpy, int screen, const char *extension) {
    const char *extensionsString, *pos;

    extensionsString = glXQueryExtensionsString(dpy, screen);

    pos = strstr(extensionsString, extension);

    return pos!=NULL && (pos==extensionsString || pos[-1]==' ') &&
        (pos[strlen(extension)]==' ' || pos[strlen(extension)]=='\0');
}
#endif

int VSyncThread::elapsed() {
    return m_timer.elapsed() / 1000;
}

void VSyncThread::setUsSyncTime(int syncTime) {
    m_usSyncTime = syncTime;
}

void VSyncThread::setVSync(bool checked) {
    m_vSync = checked;
}

int VSyncThread::usToNextSync() {
    int usRest = m_usWait - m_timer.elapsed() / 1000;
    if (usRest < 0) {
        usRest %= m_usSyncTime;
        usRest += m_usSyncTime;
    }
    return usRest;
}

int VSyncThread::rtErrorCnt() {
    return m_rtErrorCnt;
}

void VSyncThread::setSwapWait(int sw) {
    m_swapWait = sw;
}
