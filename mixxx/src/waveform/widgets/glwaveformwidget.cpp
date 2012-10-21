#include <QPainter>
#include <QGLContext>

#include "glwaveformwidget.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/renderers/waveformrenderbackground.h"
#include "waveform/renderers/qtwaveformrendererfilteredsignal.h"
#include "waveform/renderers/glwaveformrendererfilteredsignal.h"
#include "waveform/renderers/waveformrendererpreroll.h"
#include "waveform/renderers/waveformrendermark.h"
#include "waveform/renderers/waveformrendermarkrange.h"
#include "waveform/renderers/waveformrendererendoftrack.h"
#include "waveform/renderers/waveformrenderbeat.h"
#include "sharedglcontext.h"
#include "performancetimer.h"

GLWaveformWidget::GLWaveformWidget( const char* group, QWidget* parent)
        : QGLWidget(parent, SharedGLContext::getShareWidget()),
          WaveformWidgetAbstract(group) {

    addRenderer<WaveformRenderBackground>();
    addRenderer<WaveformRendererEndOfTrack>();
    addRenderer<WaveformRendererPreroll>();
    addRenderer<WaveformRenderMarkRange>();
    addRenderer<GLWaveformRendererFilteredSignal>();
    addRenderer<WaveformRenderMark>();
    addRenderer<WaveformRenderBeat>();

    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);

    setAutoBufferSwap(false);

    if (QGLContext::currentContext() != context()) {
        makeCurrent();
    }
    m_initSuccess = init();
}

GLWaveformWidget::~GLWaveformWidget() {
}

void GLWaveformWidget::castToQWidget() {
    m_widget = static_cast<QWidget*>(static_cast<QGLWidget*>(this));
}

void GLWaveformWidget::paintEvent( QPaintEvent* event) {
    Q_UNUSED(event);
}

void GLWaveformWidget::render() {
    PerformanceTimer timer;
    int t1, t2, t3;
    timer.start();
    // QPainter makes QGLContext::currentContext() == context()
    // this may delayed until previous buffer swap finished
    QPainter painter(this);
    t1 = timer.restart();
    draw(&painter, NULL);
    t2 = timer.restart();
    glFinish();
    t3 = timer.restart();
    qDebug() << "GLWaveformWidget"<< t1 << t2 << t3;
}

void GLWaveformWidget::postRender() {
    QGLWidget::swapBuffers();
}
