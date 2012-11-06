#include <QGLWidget>
#include <QGLFormat>
#include "sharedglcontext.h"
#include <qdebug.h>

// Singleton wrapper around QGLWidget

// static
QGLWidget* SharedGLContext::s_pShareWidget = NULL;

void SharedGLContext::initShareWidget(QWidget* parent) {
    if (s_pShareWidget == NULL) {
        // defaultFormat is configured in WaveformWidgetFactory::WaveformWidgetFactory();
        s_pShareWidget = new QGLWidget(QGLFormat::defaultFormat(), parent);
        s_pShareWidget->resize(1, 1);
    }
}

// static
const QGLWidget* SharedGLContext::getShareWidget() {
    return s_pShareWidget;
}
