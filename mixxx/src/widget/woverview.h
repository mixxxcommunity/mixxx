//
// C++ Interface: woverview
//
// Description:
//
//
// Author: Tue Haste Andersen <haste@diku.dk>, (C) 2003
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef WOVERVIEW_H
#define WOVERVIEW_H

#include <QPaintEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QColor>
#include <QList>

#include "trackinfoobject.h"
#include "widget/wwidget.h"

#include "waveform/renderers/waveformsignalcolors.h"
#include "waveform/renderers/waveformmarkset.h"
#include "waveform/renderers/waveformmarkrange.h"

/**
Waveform overview display
@author Tue Haste Andersen
*/

class ControlObjectThreadMain;
class Waveform;

class WOverview : public WWidget
{
    Q_OBJECT
public:
    WOverview(const char* pGroup, ConfigObject<ConfigValue>* pConfig, QWidget *parent=NULL);
    virtual ~WOverview();
    void setup(QDomNode node);

    QColor getMarkerColor();

protected:
    void mouseMoveEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void paintEvent(QPaintEvent *);
    void timerEvent(QTimerEvent *);
    void resizeEvent(QResizeEvent *);

public slots:
    void setValue(double);

    void slotLoadNewTrack(TrackPointer pTrack);
    void slotUnloadTrack(TrackPointer pTrack);

signals:
    void trackDropped(QString filename, QString group);

protected:
    virtual void dragEnterEvent(QDragEnterEvent* event);
    virtual void dropEvent(QDropEvent* event);

private slots:
    void onTotalGainChange(double v);
    void onEndOfTrackChange(double v);

    void onMarkChanged(double v);
    void onMarkRangeChange(double v);

    void slotWaveformSummaryUpdated();

  private:
    /** append the waveform overview pixmap according to available data in waveform */
    bool drawNextPixmapPart();
    inline int valueToPosition( float value) const {
        return floor(m_a * value + m_b + 0.5);
    }
    inline double positionToValue( int position) const {
        return (float(position) - m_b) / m_a;
    }

    const char* m_pGroup;
    ConfigObject<ConfigValue>* m_pConfig;
    ControlObjectThreadMain* m_totalGainControl;
    double m_totalGain;
    ControlObjectThreadMain* m_endOfTrackControl;
    double m_endOfTrack;
    ControlObjectThreadMain* m_trackSamplesControl;

    Waveform* m_waveform;
    QPixmap m_waveformPixmap;

    bool m_pixmapDone;
    float m_waveformPeak;

    /** Hold the last visual sample processed to generate the pixmap*/
    int m_actualCompletion;
    double m_visualSamplesByPixel;
    int m_renderSampleLimit;

    int m_timerPixmapRefresh;

    // Current active track
    TrackPointer m_pCurrentTrack;

    /** True if slider is dragged. Only used when m_bEventWhileDrag is false */
    bool m_bDrag;
    /** Internal storage of slider position in pixels */
    int m_iPos, m_iStartMousePos;

    QPixmap m_backgroundPixmap;
    QString m_backgroundPixmapPath;
    QColor m_qColorBackground;
    QColor m_qColorMarker;
    QColor m_endOfTrackColor;

    WaveformSignalColors m_signalColors;
    WaveformMarkSet m_marks;
    std::vector<WaveformMarkRange> m_markRanges;

    /** coefficient value-position linear transposition */
    float m_a;
    float m_b;
};

#endif
