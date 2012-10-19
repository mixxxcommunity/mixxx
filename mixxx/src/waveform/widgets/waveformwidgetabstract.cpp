#include "waveformwidgetabstract.h"
#include "waveform/renderers/waveformwidgetrenderer.h"

#include <QtDebug>
#include <QWidget>


WaveformWidgetAbstract::WaveformWidgetAbstract( const char* group) :
    WaveformWidgetRenderer(group),
    m_initSuccess(false) {
    m_widget = NULL;
}

WaveformWidgetAbstract::~WaveformWidgetAbstract() {
}

void WaveformWidgetAbstract::hold() {
    if (m_widget) {
        m_widget->hide();
    }
}

void WaveformWidgetAbstract::release() {
    if (m_widget) {
        m_widget->show();
    }
}

void WaveformWidgetAbstract::preRender(const QTime& posTime) {
    WaveformWidgetRenderer::onPreRender(posTime);
}

void WaveformWidgetAbstract::render() {
    if (m_widget) {
        if (!m_widget->isVisible()) {
            m_widget->show();
        }
        //m_widget->update();
        m_widget->repaint(); // Repaints the widget directly by calling paintEvent()
    }
}

void WaveformWidgetAbstract::resize( int width, int height) {
    if (m_widget) {
        m_widget->resize(width, height);
    }
    WaveformWidgetRenderer::resize(width, height);
}
