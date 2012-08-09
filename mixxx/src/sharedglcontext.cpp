#include <QGLWidget>
#include <QGLFormat>
#include "sharedglcontext.h"
#include <qdebug.h>

// Singleton wrapper around QGLWidget

// static
QGLWidget* SharedGLContext::s_pShareWidget = NULL;

// static
const QGLWidget* SharedGLContext::getShareWidget() {
    if (s_pShareWidget == NULL) {
        // defaultFormat is configured in WaveformWidgetFactory::WaveformWidgetFactory();
        s_pShareWidget = new QGLWidget(QGLFormat::defaultFormat());
        qDebug() << "swap intervall" << QGLFormat::defaultFormat().swapInterval();
    }
    return s_pShareWidget;
}
