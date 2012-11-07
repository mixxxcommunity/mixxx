
#include <QThread>
#include <QGLWidget>
#include <QGLFormat>
#include <QTime>
#include <qdebug.h>
#include <QTime>


#include "vsyncthread.h"
#include "performancetimer.h"

#if defined(__APPLE__)

#elif defined(__WINDOWS__)

#else
   extern const QX11Info *qt_x11Info(const QPaintDevice *pd);
#endif

VSyncThread::VSyncThread(QWidget* parent)
        : QThread(),
          m_firstRun(false),
          m_usSyncTime(33333),
          m_vSyncMode(ST_TIMER),
          m_syncOk(false),
          m_rtErrorCnt(0),
          m_swapWait(0) {
    doRendering = true;

    //QGLFormat glFormat = QGLFormat::defaultFormat();
    //glFormat.setSwapInterval(1);
    //glw = new QGLWidget(glFormat);

 //   m_glw = new QGLWidget(parent);
 //   m_glw->resize(1,1);
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

#endif
}

VSyncThread::~VSyncThread() {
    doRendering = false;
    m_sema.release(2); // Two slots
    wait();
    //delete m_glw;
}

void VSyncThread::stop()
{
    doRendering = false;
}


void VSyncThread::run() {
    QThread::currentThread()->setObjectName("VSyncThread");

 //   m_glw->makeCurrent();

    int usRest;
    int usLast;

    m_usWait = m_usSyncTime;
    m_timer.start();

    while (doRendering) {
        if (m_vSyncMode == ST_FREE) {
            // for benchmark only!
            emit(vsync1()); // renders the waveform, Possible delayed due to anti tearing
            m_sema.acquire();
            emit(vsync2()); // swaps the new waveform to front
            m_sema.acquire();
            m_timer.restart();
            m_usWait = 1000;
            usleep(1000);
        } else if (m_vSyncMode == ST_SGI_VIDEO_SYNC) {
            // This mode should be used wit vblank_mode = 0

            // sleep just before vsync
            usRest = m_usWait - m_timer.elapsed() / 1000;
            if (m_syncOk) {
                // prepare for start waiting 1 ms before Vsync by GL
                if (usRest > 5100) {
                    usleep(usRest - 5000);
                }
            } else {
                // waiting for interval by sleep
                if (usRest > 100) {
                    usleep(usRest);
                }
            }

            emit(vsync2()); // swaps the new waveform to front
            m_sema.acquire();
            // This is delayed until vsync the delay is stored in m_swapWait
            // <- Assume we are VSynced here ->
            usLast = m_timer.restart() / 1000;
            if (usRest < 0) {
                // Swapping was delayed
                // Assume the real swap happens on the following VSync
                m_rtErrorCnt++; // Count as Real Time Error
            }
            // try to stay in right intervals
            usRest = m_usWait - usLast;
            m_usWait = m_usSyncTime + (usRest % m_usSyncTime);
            // m_swapWait of 1000 µs is desired
            if (m_syncOk) {
                m_usWait += (m_swapWait - 5000); // shift interval to avoid waiting for swap again
            } else if (m_swapWait > 1000) {
                // missed swap window, delayed by driver anti tearing
                // assume we are hardware synced now
                m_usWait = m_usSyncTime;
                m_rtErrorCnt++; // Count as Real Time Error
            }
            emit(vsync1()); // renders the new waveform.
            m_sema.acquire();
            qDebug()  << "ST_SGI_VIDEO_SYNC                          " << usLast << m_swapWait << m_syncOk << usRest;
        } else if (m_vSyncMode == ST_MESA_VBLANK_MODE_1) {
            // This mode should be used wit vblank_mode = 1

            // sync by sleep
            usRest = m_usWait - m_timer.elapsed() / 1000;
            if (usRest > 100) {
                usleep(usRest);
            }

            // <- Assume we are VSynced here ->
            // now we have one VSync interval time for swap.
            usLast = m_timer.restart() / 1000;
            if (usRest < 0) {
                // Swapping was delayed
                // Assume the real swap happens on the following VSync
                m_rtErrorCnt++; // Count as Real Time Error
            }
            usRest = m_usWait - usLast;
            if (usRest < -8333) { // -8.333 ms for up to 120 Hz Displays
                // Out of syc, start new
                m_usWait = m_usSyncTime;
            } else {
                // try to stay in right intervals
                m_usWait = m_usSyncTime + (usRest % m_usSyncTime);
            }

            emit(vsync1()); // renders the waveform, Possible delayed due to anti tearing
            m_sema.acquire();
            m_usWait += m_swapWait; // shift interval to avoid waiting for swap again

            // Sleep to hit the desired interval and avoid a jitter
            // by swapping to early when using a frame rate smaler then the display refresh rate
            usRest = m_usWait - m_timer.elapsed() / 1000;
            usRest -= 8333; // -8.333 ms for up to 120 Hz Displays
            if (usRest > 100) {
                usleep(usRest);
            }
            emit(vsync2()); // swaps the new waveform to front
            m_sema.acquire();
            //qDebug()  << "ST_MESA_VBLANK_MODE_1                        " << usLast << inSync;
        } else { // if (m_vSyncMode == ST_TIMER) {
            // This mode should be used wit vblank_mode = 0
            usRest = m_usWait - m_timer.elapsed() / 1000;
            // waiting for interval by sleep
            if (usRest > 100) {
                usleep(usRest);
            }

            emit(vsync2()); // swaps the new waveform to front
            m_sema.acquire();
            // This is delayed until vsync the delay is stored in m_swapWait
            // <- Assume we are VSynced here ->
            usLast = m_timer.restart() / 1000;
            if (usRest < 0) {
                // Swapping was delayed
                // Assume the real swap happens on the following VSync
                m_rtErrorCnt++; // Count as Real Time Error
            }
            // try to stay in right intervals
            usRest = m_usWait - usLast;
            m_usWait = m_usSyncTime + (usRest % m_usSyncTime);
            emit(vsync1()); // renders the new waveform.
            m_sema.acquire();
            qDebug() << "ST_TIMER                      " << usLast << usRest;
        }
    }
}

bool VSyncThread::waitForVideoSync(QGLWidget* glw) {
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

    if (!m_firstRun) {
        initGlxext(glw);
        m_firstRun = true;
    }

    if (glXGetMscRateOML) {
        int32_t numerator;
        int32_t denominator;
        const QX11Info *xinfo = qt_x11Info(glw);
        glXGetMscRateOML(xinfo->display(), glXGetCurrentDrawable(), &numerator, &denominator);
        qDebug() << "glXGetMscRateOML" << (double)numerator/denominator << "Hz";
    }
    else
    {
        qDebug() << "glXGetMscRateOML == NULL";
    }

    if (m_vSyncMode == ST_SGI_VIDEO_SYNC) {
        if (glXGetVideoSyncSGI && glXWaitVideoSyncSGI) {
            if (!glXWaitVideoSyncSGI(1, 0, &counter)) {
                qDebug() << "counter" << counter;
                m_syncOk = true;
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
    }
#endif
    m_syncOk = false;
    return false;
}


#if defined(__APPLE__)

#elif defined(__WINDOWS__)

#else

void VSyncThread::initGlxext(QGLWidget* glw) {
    const QX11Info *xinfo = qt_x11Info(glw);
    if (glXExtensionSupported(xinfo->display(), xinfo->screen(), "GLX_SGI_video_sync")) {
        glXGetVideoSyncSGI = (PFNGLXGETVIDEOSYNCSGIPROC)glXGetProcAddressARB((const GLubyte *)"glXGetVideoSyncSGI");
        glXWaitVideoSyncSGI = (PFNGLXWAITVIDEOSYNCSGIPROC)glXGetProcAddressARB((const GLubyte *)"glXWaitVideoSyncSGI");
    }

    if (glXExtensionSupported(xinfo->display(), xinfo->screen(), "GLX_SGI_swap_control")) {
        glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
    }

    if (glXExtensionSupported(xinfo->display(), xinfo->screen(), "GLX_EXT_swap_control")) {
        glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
    }

    if (glXExtensionSupported(xinfo->display(), xinfo->screen(), "GLX_OML_sync_control")) {
        glXGetSyncValuesOML = (PFNGLXGETSYNCVALUESOMLPROC)glXGetProcAddressARB((const GLubyte *)"glXGetSyncValuesOML");
        glXGetMscRateOML = (PFNGLXGETMSCRATEOMLPROC)glXGetProcAddressARB((const GLubyte *)"glXGetMscRateOML");
        glXSwapBuffersMscOML = (PFNGLXSWAPBUFFERSMSCOMLPROC)glXGetProcAddressARB((const GLubyte *)"glXSwapBuffersMscOML");
        glXWaitForMscOML = (PFNGLXWAITFORMSCOMLPROC)glXGetProcAddressARB((const GLubyte *)"glXWaitForMscOML");
        glXWaitForSbcOML = (PFNGLXWAITFORSBCOMLPROC)glXGetProcAddressARB((const GLubyte *)"glXWaitForSbcOML");
    }
}

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

void VSyncThread::setVSyncType(int type) {
    if (type >= (int)VSyncThread::ST_COUNT) {
        type = VSyncThread::ST_TIMER;
    }
    m_vSyncMode = (enum VSyncMode)type;
    m_rtErrorCnt = 0;
}

int VSyncThread::usToNextSync() {
    int usRest = m_usWait - m_timer.elapsed() / 1000;
    if (usRest < 0) {
        usRest %= m_usSyncTime;
        usRest += m_usSyncTime;
    }
    return usRest;
}

int VSyncThread::usFromTimerToNextSync(PerformanceTimer* timer) {
    int difference = m_timer.difference(timer) / 1000;
    return difference + m_usWait;
}

int VSyncThread::rtErrorCnt() {
    return m_rtErrorCnt;
}

void VSyncThread::setSwapWait(int sw) {
    m_swapWait = sw;
}

void VSyncThread::vsyncSlotFinished() {
    m_sema.release();
}

void VSyncThread::getAvailableVSyncTypes(QList<QPair<int, QString > >* pList) {
    QPair<int, QString > pair;

    for (int i = (int)VSyncThread::ST_TIMER; i < (int)VSyncThread::ST_COUNT; i++) {
        //if (isAvailable(type))  // TODO
        {
            enum VSyncMode mode = (enum VSyncMode)i;

            QString name;
            switch (mode) {
            case VSyncThread::ST_TIMER:
                name = tr("Timer (Fallback)");
                break;
            case VSyncThread::ST_MESA_VBLANK_MODE_1:
                name = tr("MESA vblank_mode = 1");
                break;
            case VSyncThread::ST_SGI_VIDEO_SYNC:
                name = tr("Wait for Video sync");
                break;
            case VSyncThread::ST_OML_SYNC_CONTROL:
                name = tr("Sync Control");
                break;
            case VSyncThread::ST_FREE:
                name = tr("Free + 1 ms (for benchmark only)");
                break;
            default:
                break;
            }
            QPair<int, QString > pair = QPair<int, QString >(i, name);
            pList->append(pair);
        }
    }
}
