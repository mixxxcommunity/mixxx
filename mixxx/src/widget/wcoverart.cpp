#include "wcoverart.h"
#include "wwidget.h"
#include "wskincolor.h"
#include <QDebug>
#include <QPainter>
#include <QLabel>
#include <QBitmap>
#include <QApplication>

WCoverArt::WCoverArt(ConfigObject<ConfigValue>* pConfig, QWidget *parent)
                    : QWidget(parent), m_pConfig(pConfig) {
    m_coverIsVisible = m_pConfig->getValueString(ConfigKey("[Library]", "ShowCover")).toInt();
    m_coverIsHovered = 0;
    m_coverIsEmpty = 1;
    m_currentCover = "";
    loadCover(m_currentCover);
}

WCoverArt::~WCoverArt() {
     m_pConfig->set(ConfigKey("[Library]", "ShowCover"),
                    ConfigValue((int)m_coverIsVisible));
}

void WCoverArt::setup(QDomNode node) {
    Q_UNUSED(node);
    setMouseTracking(TRUE);

    // Background color
    QColor bgc(255,255,255);
    if (!WWidget::selectNode(node, "BgColor").isNull()) {
        bgc.setNamedColor(WWidget::selectNodeQString(node, "BgColor"));
        setAutoFillBackground(true);
    }
    QPalette pal = palette();
    pal.setBrush(backgroundRole(), WSkinColor::getCorrectColor(bgc));

    // Foreground color
    m_fgc = QColor(0,0,0);
    if (!WWidget::selectNode(node, "FgColor").isNull()) {
        m_fgc.setNamedColor(WWidget::selectNodeQString(node, "FgColor"));
    }
    bgc = WSkinColor::getCorrectColor(bgc);
    m_fgc = QColor(255 - bgc.red(), 255 - bgc.green(), 255 - bgc.blue());
    pal.setBrush(foregroundRole(), m_fgc);
    setPalette(pal);
}

void WCoverArt::loadCover(QString img) {
    // set default cover
    if (img.isEmpty()) {
        m_currentCover = ":/images/library/vinyl-record.png";
        m_coverIsEmpty = 1;
    }
    else {
        m_currentCover = img;
        m_coverIsEmpty = 0;
    }
    update();
}

void WCoverArt::clearCover(const QString& img) {
    Q_UNUSED(img);
    m_currentCover = ":/images/library/vinyl-record.png";
    m_coverIsEmpty = 1;

    update();
}

void WCoverArt::paintEvent(QPaintEvent *) {
    QPainter painter(this);

    painter.drawLine(0,0,width(),0);

    if (m_coverIsVisible) {
        //painter.drawRect(width()/2-(height()/2)-1, 5, height()-9, height()-9);
        m_coverArt.load(m_currentCover);
        m_coverArt = m_coverArt.scaled(QSize(height()-10, height()-10),
                                       Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
        painter.drawImage(width()/2-height()/2, 6, m_coverArt);
    }
    else {
        QImage sc = QImage(":/images/library/ic_library_cover_show.png");
        sc = sc.scaled(height()-1, height()-1,
                       Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation);
        painter.drawImage(0, 1 ,sc);
        painter.drawText(25, 15, tr("Show cover"));
    }

    if (m_coverIsVisible && m_coverIsHovered) {
        QImage hc = QImage(":/images/library/ic_library_cover_hide.png");
        hc = hc.scaled(20, 20,
                       Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation);
        painter.drawImage(width()-21, 6, hc);
    }
}

void WCoverArt::resizeEvent(QResizeEvent *) {
    if (m_coverIsVisible)
        setMinimumSize(0, parentWidget()->height()/3);
    else
        setMinimumSize(0, 20);
}

void WCoverArt::mousePressEvent(QMouseEvent *event) {
    QPoint lastPoint;
    lastPoint = event->pos();
    if (m_coverIsVisible) {
        if(lastPoint.x() > width()-(height()/5) && lastPoint.y() < (height()/5)+5) {
            m_coverIsVisible = 0;
            resize(sizeHint());
        }
        else {
            if (!m_coverIsEmpty) {
                QLabel *lb = new QLabel(this, Qt::Popup | Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);
                lb->setWindowModality(Qt::ApplicationModal);
                int index = m_currentCover.lastIndexOf("/");
                QString title = m_currentCover.mid(index+1);
                lb->setWindowTitle(title);

                QPixmap px = QPixmap(m_currentCover);
                QSize sz = QApplication::activeWindow()->size();

                if (px.height() > sz.height()/1.2) {
                    px = px.scaledToHeight(sz.height()/1.2, Qt::SmoothTransformation);
                }

                lb->setPixmap(px);
                lb->setGeometry(sz.width()/2-px.width()/2, sz.height()/2-px.height()/2.2,
                                px.width(), px.height());
                lb->show();
            }
        }
    }
    else {
        m_coverIsVisible = 1;
        setCursor(Qt::ArrowCursor);
        resize(sizeHint());
    }
}

void WCoverArt::mouseMoveEvent(QMouseEvent *event) {
    QPoint lastPoint;
    lastPoint = event->pos();
    if (event->HoverEnter) {
        m_coverIsHovered  = 1;
        if (m_coverIsVisible) {
            if (lastPoint.x() > width()-(height()/5) && lastPoint.y() < (height()/5)+5)
                setCursor(Qt::ArrowCursor);
            else {
                if (m_coverIsEmpty) {
                    setCursor(Qt::ArrowCursor);
                }
                else {
                    QPixmap pix(":/images/library/ic_library_zoom_in.png");
                    pix = pix.scaled(24, 24);
                    setCursor(QCursor(pix));
                }
            }

        }
        else {
            setCursor(Qt::PointingHandCursor);
        }
    }
    update();
}

void WCoverArt::leaveEvent(QEvent *) {
    m_coverIsHovered = 0;
    update();
}

