
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
          m_rtErrorCnt(0) {
    doRendering = true;

    //QGLFormat glFormat = QGLFormat::defaultFormat();
    //glFormat.setSwapInterval(1);
    //glw = new QGLWidget(glFormat);

    glw = new QGLWidget(parent);

    //qDebug() << glw->size();

    //glw->moveToThread(this);

    glw->resize(1,1);
    glw->hide();


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
    int usWait = m_usSyncTime;
    int usLast;

    bool vSync = m_vSync;


    m_timer.start();

    while (doRendering) {
        if (m_vSync) {
            if (!waitForVideoSync()){
                // Driver does not support wait for vsync
                usRest = usWait - m_timer.elapsed() / 1000;
                if (usRest > 1) {
                    usleep(usRest);
                }
            }
        } else {
            usRest = usWait - m_timer.elapsed() / 1000;
            if (usRest > 1) {
                usleep(usRest);
            }
        }

        usLast = m_timer.restart() / 1000;
        qDebug()  << "VSync 4                           " << usLast;
        emit(vsync());
        usWait -= usLast;
        if (usWait < (-1 * m_usSyncTime)) {
            m_rtErrorCnt++;
            if (m_vSync) {
                // try to stay in right intervals
                usWait %= m_usSyncTime;
            } else {
                // start from new
                usWait = 0;
            }
        }
        usWait += m_usSyncTime;
    }
}

bool VSyncThread::waitForVideoSync() {
    uint counter;

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
        counter = m_counter;
        if (!glXGetVideoSyncSGI(&counter)) {
            glXWaitVideoSyncSGI(2, (counter + 1) % 2, &counter);
            if (counter < m_counter)
                fprintf(stderr, "error:  vblank count went backwards: %d -> %d\n", m_counter, counter);
            if (counter == m_counter)
                fprintf(stderr, "error:  count didn't change: %d\n", counter);
            if (counter > m_counter + 1)
                fprintf(stderr, "error:  one counter lost: %d\n", counter);
            m_counter = counter;
            return true;
        }
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
    return m_usSyncTime - (elapsed() % m_usSyncTime);
}

int VSyncThread::rtErrorCnt() {
    return m_rtErrorCnt;
}
