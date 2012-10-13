#include "waveformrenderbackground.h"
#include "waveformwidgetrenderer.h"

#include "widget/wskincolor.h"
#include "widget/wwidget.h"

WaveformRenderBackground::WaveformRenderBackground(
    WaveformWidgetRenderer* waveformWidgetRenderer)
        : WaveformRendererAbstract(waveformWidgetRenderer),
          m_backgroundColor(0, 0, 0) {
}

WaveformRenderBackground::~WaveformRenderBackground() {
}

void WaveformRenderBackground::setup(const QDomNode& node) {
    m_backgroundColor.setNamedColor(
        WWidget::selectNodeQString(node, "BgColor"));
    m_backgroundColor = WSkinColor::getCorrectColor(m_backgroundColor);
    m_backgroundPixmapPath = WWidget::selectNodeQString(node, "BgPixmap");
    setDirty(true);
}

void WaveformRenderBackground::draw(QPainter* painter,
                                    QPaintEvent* /*event*/) {
    if (isDirty()) {
        generatePixmap();
    }

    // since we use opaque widget we need to draw the background !
    //painter->drawPixmap(QPoint(0, 0), m_backgroundPixmap);

    // QImage image = pixmap.toImage();
    // If the system depth is 16 and the pixmap doesn't have an alpha channel
    // then we convert it to RGB16 in the hope that it gets uploaded as a 16
    // bit texture which is much faster to access than a 32-bit one.
    //if (pixmap.depth() == 16 && !image.hasAlphaChannel() )
    //    image = image.convertToFormat(QImage::Format_RGB16);

    //58 µs (QWidget eePC)
    //151 µs (QGlWidget eePC)
    //painter->fillRect(m_backgroundImage.rect(), m_backgroundColor);

    // 731 µs (QWidget Intel Qt 4.6)
    // 166 µs (QGlWidget Intel Qt 4.6)
    // 170 µs (QGlWidget Radeon Qt 4.8)
    // 161 µs (QWidget Radeon Qt 4.8)
    painter->drawImage(QPoint(0, 0), m_backgroundImage);

    // This produces a white back ground with Linux QT 4.6 QGlWidget and Intel i915 driver
    // time for such a setup:
    // 38 µs (QWidget Intel Qt 4.6)
    // 549 µs (QGlWidget Intel Qt 4.6)
    // 355 µs (QGlWidget Radeon Qt 4.8)
    // 174 µs (QWidget Radeon Qt 4.8)
    //painter->drawPixmap(QPoint(0, 0), m_backgroundPixmap);

    // 9089 µs (QGlWidget Intel Qt 4.6)
    //painter->drawImage(QPoint(0, 0), m_backgroundPixmap.toImage());
}

void WaveformRenderBackground::generatePixmap() {
    if (m_backgroundPixmapPath != "") {
        QPixmap backgroundPixmap(WWidget::getPath(m_backgroundPixmapPath));

        if (!backgroundPixmap.isNull()){
            if (backgroundPixmap.width() == m_waveformRenderer->getWidth() &&
                    backgroundPixmap.height() == m_waveformRenderer->getHeight()) {
                m_backgroundPixmap = backgroundPixmap;
            } else {
                qWarning() << "WaveformRenderBackground::generatePixmap - file("
                           << WWidget::getPath(m_backgroundPixmapPath)
                           << ")" << backgroundPixmap.width()
                           << "x" << backgroundPixmap.height()
                           << "do not fit the waveform widget size"
                           << m_waveformRenderer->getWidth()
                           << "x" << m_waveformRenderer->getHeight();

                m_backgroundPixmap = QPixmap(m_waveformRenderer->getWidth(),
                                             m_waveformRenderer->getHeight());
                QPainter painter(&m_backgroundPixmap);
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                painter.drawPixmap(m_backgroundPixmap.rect(),
                                   backgroundPixmap, backgroundPixmap.rect());
            }
        } else {
            qWarning() << "WaveformRenderBackground::generatePixmap - file("
                       << WWidget::getPath(m_backgroundPixmapPath)
                       << ") is not valid ...";
            m_backgroundPixmap = QPixmap(m_waveformRenderer->getWidth(),
                                         m_waveformRenderer->getHeight());
            m_backgroundPixmap.fill(m_backgroundColor);
        }
    } else {
        qWarning() << "WaveformRenderBackground::generatePixmap - no background file";
        m_backgroundPixmap = QPixmap(m_waveformRenderer->getWidth(),
                                     m_waveformRenderer->getHeight());
        m_backgroundPixmap.fill(m_backgroundColor);
    }
    m_backgroundImage = m_backgroundPixmap.toImage();


    setDirty(false);
}
