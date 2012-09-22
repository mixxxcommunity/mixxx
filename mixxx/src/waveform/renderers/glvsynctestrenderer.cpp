#include "glvsynctestrenderer.h"

#include "waveformwidgetrenderer.h"
#include "waveform/waveform.h"

#include "waveform/waveformwidgetfactory.h"

#include <qgl.h>

GLVSyncTestRenderer::GLVSyncTestRenderer(
        WaveformWidgetRenderer* waveformWidgetRenderer)
    : WaveformRendererSignalBase(waveformWidgetRenderer),
      m_drawcount(0) {

}

GLVSyncTestRenderer::~GLVSyncTestRenderer() {
}

void GLVSyncTestRenderer::onSetup(const QDomNode &node) {
    Q_UNUSED(node);
}

inline void setPoint(QPointF& point, qreal x, qreal y) {
    point.setX(x);
    point.setY(y);
}

void GLVSyncTestRenderer::draw(QPainter* painter, QPaintEvent* /*event*/) {

    TrackPointer pTrack = m_waveformRenderer->getTrackInfo();
    if (!pTrack) {
        return;
    }

    const Waveform* waveform = pTrack->getWaveform();
    if (waveform == NULL) {
        return;
    }

    const int dataSize = waveform->getDataSize();
    if (dataSize <= 1) {
        return;
    }

    const WaveformData* data = waveform->data();
    if (data == NULL) {
        return;
    }

    double firstVisualIndex = m_waveformRenderer->getFirstDisplayedPosition() * dataSize;
    double lastVisualIndex = m_waveformRenderer->getLastDisplayedPosition() * dataSize;

    const int firstIndex = int(firstVisualIndex+0.5);
    firstVisualIndex = firstIndex - firstIndex%2;

    const int lastIndex = int(lastVisualIndex+0.5);
    lastVisualIndex = lastIndex + lastIndex%2;

    // Reset device for native painting
    painter->beginNativePainting();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const QColor& color = m_colors.getSignalColor();

    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();
    const double visualGain = factory->getVisualGain(::WaveformWidgetFactory::All);

    float maxAll[2];

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    if (m_alignment == Qt::AlignCenter) {
        glOrtho(firstVisualIndex, lastVisualIndex, -255.0, 255.0, -10.0, 10.0);
    } else if (m_alignment == Qt::AlignBottom) {
        glOrtho(firstVisualIndex, lastVisualIndex, 0.0, 255.0, -10.0, 10.0);
    } else {
        glOrtho(firstVisualIndex, lastVisualIndex, 255.0, 0.0, -10.0, 10.0);
    }

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    if (++m_drawcount & 1) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glColor3f(1.0f, 1.0f, 1.0f);
    } else {
        glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
        glColor3f(1.0f, 0.0f, 0.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glRectf(0, 0, 1, 1);


    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    painter->endNativePainting();
}
