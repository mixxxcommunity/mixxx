
#include <QThread>
#include <QGLWidget>
#include <QGLFormat>
#include <QTime>
#include <qdebug.h>

#include "vsyncthread.h"

VSyncThread::VSyncThread()
        : QThread() {
    doRendering = true;

    QGLFormat glFormat = QGLFormat::defaultFormat();
    glFormat.setSwapInterval(1);
    glw = new QGLWidget(glFormat);
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

    while (doRendering) {
        // Wait vor VSync
        //qDebug()  << "VSync" << QTime::currentTime().msec() << glw->format().swapInterval();
        glw->swapBuffers(); // sleeps until vsync
        //emit(vsync());
        qDebug() << "VSync" << QTime::currentTime().msec();
        //msleep(10); // additional sleep in case of waiting for vsync fails
    }
}



