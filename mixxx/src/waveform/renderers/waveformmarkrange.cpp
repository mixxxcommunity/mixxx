#include <QPainter>
#include <QDebug>

#include "waveformmarkrange.h"

#include "controlobject.h"
#include "controlobjectthreadmain.h"
#include "xmlparse.h"

ControlObjectThreadMain* maybeMakeControl(ControlObject* pControl) {
    if (pControl) {
        return new ControlObjectThreadMain(pControl);
    }
    return NULL;
}

WaveformMarkRange::WaveformMarkRange()
        : m_markStartPointControl(NULL),
          m_markEndPointControl(NULL),
          m_markEnabledControl(NULL) {
}

bool WaveformMarkRange::active() {
    const double startValue = start();
    const double endValue = end();
    return startValue != endValue && startValue != -1.0 && endValue != -1.0;
}

bool WaveformMarkRange::enabled() {
    // Default to enabled if there is no enabled control.
    return m_markEnabledControl == NULL ||
            m_markEnabledControl->get() > 0.0;
}

double WaveformMarkRange::start() {
    return m_markStartPointControl ? m_markStartPointControl->get() : -1.0;
}

double WaveformMarkRange::end() {
    return m_markEndPointControl ? m_markEndPointControl->get() : -1.0;
}

void WaveformMarkRange::setup(const QString& group, const QDomNode& node) {
    m_activeColor = XmlParse::selectNodeQString(node, "Color");
    if (m_activeColor == "") {
        //vRince kind of legacy fallback ...
        // As a fallback, grab the mark color from the parent's MarkerColor
        m_activeColor = XmlParse::selectNodeQString(node.parentNode(), "MarkerColor");
        qDebug() << "Didn't get mark Color, using parent's MarkerColor:" << m_activeColor;
    }

    m_disabledColor = XmlParse::selectNodeQString(node, "DisabledColor");
    if (m_disabledColor == "") {
        //vRince kind of legacy fallback ...
        // Read the text color, otherwise use the parent's SignalColor.
        m_disabledColor = XmlParse::selectNodeQString(node.parentNode(), "SignalColor");
        qDebug() << "Didn't get mark TextColor, using parent's SignalColor:" << m_disabledColor;
    }

    m_markStartPointControl = maybeMakeControl(ControlObject::getControl(
        ConfigKey(group, XmlParse::selectNodeQString(node, "StartControl"))));
    m_markEndPointControl = maybeMakeControl(ControlObject::getControl(
        ConfigKey(group, XmlParse::selectNodeQString(node, "EndControl"))));
    m_markEnabledControl = maybeMakeControl(ControlObject::getControl(
        ConfigKey(group, XmlParse::selectNodeQString(node, "EnabledControl"))));
}

void WaveformMarkRange::generatePixmap(int weidth, int height) {
    m_activePixmap = QPixmap(weidth, height);
    m_disabledPixmap = QPixmap(weidth, height);

    //fill needed cause they remain transparent
    m_activePixmap.fill(QColor(0,0,0,0));
    m_disabledPixmap.fill(QColor(0,0,0,0));

    QColor activeColor = m_activeColor;
    activeColor.setAlphaF(0.3);
    QBrush brush(activeColor);

    QPainter painter;
    painter.begin(&m_activePixmap);
    painter.fillRect(m_activePixmap.rect(), brush);
    painter.end();

    QColor disabledColor = m_disabledColor;
    disabledColor.setAlphaF(0.3);
    brush = QBrush(disabledColor);

    painter.begin(&m_disabledPixmap);
    painter.fillRect(m_disabledPixmap.rect(), brush);
    painter.end();
}
