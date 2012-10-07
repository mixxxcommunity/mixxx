#include "glvsynctestwidget.h"

#include <QPainter>

#include "sharedglcontext.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/renderers/waveformrenderbackground.h"
#include "waveform/renderers/glwaveformrenderersimplesignal.h"
#include "waveform/renderers/glvsynctestrenderer.h"
#include "waveform/renderers/waveformrendererpreroll.h"
#include "waveform/renderers/waveformrendermark.h"
#include "waveform/renderers/waveformrendermarkrange.h"
#include "waveform/renderers/waveformrendererendoftrack.h"
#include "waveform/renderers/waveformrenderbeat.h"

GLVSyncTestWidget::GLVSyncTestWidget( const char* group, QWidget* parent)
    : QGLWidget(parent, SharedGLContext::getShareWidget()),
      WaveformWidgetAbstract(group) {

    addRenderer<WaveformRenderBackground>(); // 9089 µs
    addRenderer<WaveformRendererEndOfTrack>(); // 677 µs
    addRenderer<WaveformRendererPreroll>(); // 2034 µs
    addRenderer<WaveformRenderMarkRange>(); // 793 µs
    addRenderer<GLVSyncTestRenderer>(); // 841 µs
    addRenderer<WaveformRenderMark>(); // 711 µs
    addRenderer<WaveformRenderBeat>(); // 1183 µs

    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);

    setAutoBufferSwap(false);

    if (QGLContext::currentContext() != context()) {
        makeCurrent();
    }
    m_initSuccess = init();
}

GLVSyncTestWidget::~GLVSyncTestWidget(){
    if (QGLContext::currentContext() != context()) {
        makeCurrent();
    }
}

void GLVSyncTestWidget::castToQWidget() {
    m_widget = static_cast<QWidget*>(static_cast<QGLWidget*>(this));
}

void GLVSyncTestWidget::paintEvent( QPaintEvent* event) {
    if (QGLContext::currentContext() != context()) {
        makeCurrent();
    }
    QPainter painter(this);
    draw(&painter,event);
}

void GLVSyncTestWidget::postRender() {
    QGLWidget::swapBuffers();
}
