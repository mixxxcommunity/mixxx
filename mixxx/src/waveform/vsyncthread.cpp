
#include <QThread>
#include <QGLWidget>
#include <QGLFormat>
#include <QTime>
#include <qdebug.h>
#include <QTime>
#include <GL/glx.h>

#include "vsyncthread.h"
#include "vsync.h"
#include "performancetimer.h"


extern const QX11Info *qt_x11Info(const QPaintDevice *pd);

VSyncThread::VSyncThread(QWidget* parent)
        : QThread() {
    doRendering = true;

    //QGLFormat glFormat = QGLFormat::defaultFormat();
    //glFormat.setSwapInterval(1);
    //glw = new QGLWidget(glFormat);

    glw = new QGLWidget(parent);

    qDebug() << glw->size();

    glw->moveToThread(this);

    //glw->resize(255,255);
    //glw->show();

    const QX11Info *xinfo = qt_x11Info(glw);
    if (glXExtensionSupported(xinfo->display(), xinfo->screen(), "GLX_SGI_video_sync")) {
        glXGetVideoSyncSGI =  (qt_glXGetVideoSyncSGI)glXGetProcAddressARB((const GLubyte *)"glXGetVideoSyncSGI");
        glXWaitVideoSyncSGI = (qt_glXWaitVideoSyncSGI)glXGetProcAddressARB((const GLubyte *)"glXWaitVideoSyncSGI");
    }

    if (glXExtensionSupported(xinfo->display(), xinfo->screen(), "GLX_SGI_swap_control")) {
        glXSwapIntervalSGI =  (qt_glXSwapIntervalSGI)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
    }
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
    // glw->makeCurrent();


    PerformanceTimer timer;

    timer.start();

    while (doRendering) {
        qDebug()  << "VSync 1" << timer.elapsed();
        if (!waitForVideoSync()){
            usleep(16600);
        }
        //qDebug()  << "VSync 3" << timer.elapsed();
        //m_vsync.waitForVideoSync();
        qDebug()  << "VSync 4                           " << timer.restart();
       // emit(vsync());
        //qDebug()  << "VSync 1" << timer.elapsed();

        // Wait vor VSync
        //qDebug()  << "VSync" << QTime::currentTime().msec() << glw->format().swapInterval();
        // glw->swapBuffers(); // sleeps until vsync
        //emit(vsync());
        // qDebug() << "VSync" << QTime::currentTime().msec();
        //msleep(10); // additional sleep in case of waiting for vsync fails
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

    if (glXGetVideoSyncSGI && glXWaitVideoSyncSGI) {
        //if (!glXGetVideoSyncSGI(&counter)) {
            glXWaitVideoSyncSGI(2, (counter + 1) % 2, &counter);
            if (counter < m_counter)
                fprintf(stderr, "error:  vblank count went backwards: %d -> %d\n", m_counter, counter);
            if (counter == m_counter)
                fprintf(stderr, "error:  count didn't change: %d\n", counter);
            if (counter > m_counter + 1)
                fprintf(stderr, "error:  one counter lost: %d\n", counter);
            m_counter = counter;
            return true;
        //}
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
        return true;
    }
    return false;
}

// from mesa-demos-8.0.1/src/xdemos/glsync.c (MIT license)
bool VSyncThread::glXExtensionSupported(Display *dpy, int screen, const char *extension) {
    const char *extensionsString, *pos;

    extensionsString = glXQueryExtensionsString(dpy, screen);

    pos = strstr(extensionsString, extension);

    return pos!=NULL && (pos==extensionsString || pos[-1]==' ') &&
        (pos[strlen(extension)]==' ' || pos[strlen(extension)]=='\0');
}


