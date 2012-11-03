#include "waveformrendererhsv.h"

#include "waveformwidgetrenderer.h"
#include "waveform/waveform.h"
#include "waveform/waveformwidgetfactory.h"

#include "widget/wskincolor.h"
#include "trackinfoobject.h"
#include "widget/wwidget.h"

#include "defs.h"

#include "controlobjectthreadmain.h"

WaveformRendererHSV::WaveformRendererHSV(
        WaveformWidgetRenderer* waveformWidgetRenderer)
    : WaveformRendererSignalBase( waveformWidgetRenderer) {
}

WaveformRendererHSV::~WaveformRendererHSV() {
}

void WaveformRendererHSV::onSetup(const QDomNode& node) {
    Q_UNUSED(node);
}

void WaveformRendererHSV::draw(QPainter* painter,
                                          QPaintEvent* /*event*/) {
    const TrackPointer trackInfo = m_waveformRenderer->getTrackInfo();
    if (!trackInfo) {
        return;
    }

    const Waveform* waveform = trackInfo->getWaveform();
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

    int samplesPerPixel = m_waveformRenderer->getVisualSamplePerPixel();
    int numberOfSamples = m_waveformRenderer->getWidth() * samplesPerPixel;

    int currentPosition = 0;

    //TODO (vRince) not really accurate since waveform size une visual reasampling and
    //have two mores samples to hold the complete visual data
    currentPosition = m_waveformRenderer->getPlayPos() * dataSize;
    m_waveformRenderer->regulateVisualSample(currentPosition);

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing, false);
    painter->setRenderHints(QPainter::HighQualityAntialiasing, false);
    painter->setRenderHints(QPainter::SmoothPixmapTransform, false);
    painter->setWorldMatrixEnabled(false);
    painter->resetTransform();

    float allGain = m_waveformRenderer->getGain();

    // Save HSV of waveform color
    double h,s,v;
    m_colors.getLowColor().getHsvF(&h,&s,&v);

    QColor color;
    float lo, hi;

    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();
    allGain *= factory->getVisualGain(::WaveformWidgetFactory::All);

    const float halfHeight = (float)m_waveformRenderer->getHeight()/2.0;

    const float heightFactor = m_alignment == Qt::AlignCenter
            ? allGain*halfHeight/255.0
            : allGain*m_waveformRenderer->getHeight()/255.0;

    //draw reference line
    if( m_alignment == Qt::AlignCenter) {
        painter->setPen(m_axesColor);
        painter->drawLine(0,halfHeight,m_waveformRenderer->getWidth(),halfHeight);
    }

    for (int i = 0; i < numberOfSamples; i += samplesPerPixel) {
        const int xPos = i/samplesPerPixel;
        const int visualIndex = currentPosition + 2*i - numberOfSamples;
        if (visualIndex >= 0 && (visualIndex+1) < dataSize) {
            unsigned char maxLow[2] = {0, 0};
            unsigned char maxMid[2] = {0, 0};
            unsigned char maxHigh[2] = {0, 0};
            unsigned char maxGain[2] = {0, 0};

            for (int subIndex = 0;
                 subIndex < 2*samplesPerPixel &&
                 visualIndex + subIndex + 1 < dataSize;
                 subIndex += 2) {
                const WaveformData& waveformData = *(data + visualIndex + subIndex);
                const WaveformData& waveformDataNext = *(data + visualIndex + subIndex + 1);
                maxLow[0] = math_max(maxLow[0], waveformData.filtered.low);
                maxLow[1] = math_max(maxLow[1], waveformDataNext.filtered.low);
                maxMid[0] = math_max(maxMid[0], waveformData.filtered.mid);
                maxMid[1] = math_max(maxMid[1], waveformDataNext.filtered.mid);
                maxHigh[0] = math_max(maxHigh[0], waveformData.filtered.high);
                maxHigh[1] = math_max(maxHigh[1], waveformDataNext.filtered.high);
                maxGain[0] = math_max(maxGain[0], waveformData.filtered.all)/1.5;
                maxGain[1] = math_max(maxGain[1], waveformDataNext.filtered.all)/1.5;
            }

            if( maxLow[0] && maxLow[1] && maxMid[0] && maxMid[1] && maxHigh[0] && maxHigh[1] ) {
                lo = maxLow[0]/300.0;
                hi = maxHigh[0]/200.0;

                // Too black/white/gray color
                if( lo+hi > 0.8 )
                {
                    lo /= (lo+hi)/0.8;
                    hi /= (lo+hi)/0.8;
                }
                color.setHsvF(h, 1.0-hi, 1.0-lo);

                painter->setPen(color);
                painter->drawLine(
                            (int)xPos, (int)(halfHeight-heightFactor*(float)maxGain[0]),
                            (int)xPos, (int)(halfHeight+heightFactor*(float)maxGain[1]) );
            }
        }
    }
    painter->restore();
}
