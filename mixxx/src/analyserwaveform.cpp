#include <QImage>
#include <QtDebug>
#include <QTime>
#include <QMutexLocker>
#include <QDebug>
#include <time.h>

#include "analyserwaveform.h"
#include "engine/engineobject.h"
#include "engine/enginefilterbutterworth8.h"
#include "engine/enginefilteriir.h"
#include "library/trackcollection.h"
#include "library/dao/analysisdao.h"
#include "trackinfoobject.h"
#include "waveform/waveformfactory.h"

AnalyserWaveform::AnalyserWaveform(ConfigObject<ConfigValue>* pConfig) :
        m_skipProcessing(false),
        m_waveform(NULL),
        m_waveformSummary(NULL),
        m_waveformDataSize(0),
        m_currentSummaryStride(0),
        m_currentStride(0),
        m_waveformData(NULL),
        m_waveformSummaryData(NULL),
        m_waveformSummaryDataSize(0) {
    qDebug() << "AnalyserWaveform::AnalyserWaveform()";

    m_filter[0] = 0;
    m_filter[1] = 0;
    m_filter[2] = 0;

    static int i = 0;
    m_database = QSqlDatabase::addDatabase("QSQLITE", "WAVEFORM_ANALYSIS" + QString::number(i++));
    if (!m_database.isOpen()) {
        m_database.setHostName("localhost");
        m_database.setDatabaseName(pConfig->getSettingsPath().append("/mixxxdb.sqlite"));
        m_database.setUserName("mixxx");
        m_database.setPassword("mixxx");

        //Open the database connection in this thread.
        if (!m_database.open()) {
            qDebug() << "Failed to open database from analyser thread."
                     << m_database.lastError();
        }
    }

    m_timer = new QTime();
    m_analysisDao = new AnalysisDao(m_database);
}

AnalyserWaveform::~AnalyserWaveform() {
    qDebug() << "AnalyserWaveform::~AnalyserWaveform()";
    destroyFilters();
    m_database.close();
    delete m_timer;
    delete m_analysisDao;
}

bool AnalyserWaveform::initialise(TrackPointer tio, int sampleRate, int totalSamples) {
    m_skipProcessing = false;

    m_timer->start();
    m_waveform = tio->getWaveform();
    m_waveformSummary = tio->getWaveformSummary();

    if (!m_waveform || !m_waveformSummary || totalSamples == 0) {
        qWarning() << "AnalyserWaveform::initialise - no waveform/waveform summary";
        return false;
    }

    // If we don't need to calculate the waveform/wavesummary, skip.
    if (loadStored(tio)) {
        m_skipProcessing = true;
    } else {
        // Now actually initalise the AnalyserWaveform:
        QMutexLocker waveformLocker(m_waveform->getMutex());
        QMutexLocker waveformSummaryLocker(m_waveformSummary->getMutex());

        destroyFilters();
        resetFilters(tio, sampleRate);

        //TODO (vrince) Do we want to expose this as settings or whatever ?
        const double mainWaveformSampleRate = 441;
        m_waveform->computeBestVisualSampleRate(sampleRate, mainWaveformSampleRate);

        //two visual sample per pixel in full width overview in full hd
        int summaryWaveformSamples = 2*1920;
        double summaryWaveformSampleRate = (double)summaryWaveformSamples * (double)sampleRate / (double)totalSamples;
        double summaryMainRatio = summaryWaveformSampleRate / m_waveform->getVisualSampleRate();
 //       if (summaryWaveformSampleRate > m_waveform->getVisualSampleRate()) {
 //           double summaryMainRatio = ceil(summaryWaveformSampleRate / m_waveform->getVisualSampleRate());
 //       } else {

 //       }

        summaryWaveformSampleRate = summaryMainRatio * m_waveform->getVisualSampleRate();
        m_waveformSummary->computeBestVisualSampleRate(sampleRate, summaryWaveformSampleRate);

        // getDataSize() of both waveform and waveformSummary are now totalSamples
        m_waveform->allocateForAudioSamples(totalSamples);
        m_waveformSummary->allocateForAudioSamples(totalSamples);
        m_waveformDataSize = m_waveform->getDataSize();
        m_waveformSummaryDataSize = m_waveformSummary->getDataSize();

        m_waveformData = &m_waveform->at(0);
        m_waveformSummaryData = &m_waveformSummary->at(0);

        m_stride.reset();
        m_strideSummary.reset();

        const double mainSummaryRatio = m_waveform->getVisualSampleRate() / m_waveformSummary->getVisualSampleRate();
        if (mainSummaryRatio < 1) {
            const int summaryStrideLength = (int)(1.0/mainSummaryRatio);
            m_strideSummary.init(m_waveformSummary->getAudioVisualRatio());
            m_stride.init(summaryStrideLength);
            m_stride.m_conversionFactor = (double)mainSummaryRatio; // required, because sumary is the everage of maxes
        } else {
            const int summaryStrideLength = ceil(mainSummaryRatio);
            m_stride.init((int)m_waveform->getAudioVisualRatio());
            m_strideSummary.init(summaryStrideLength);
            m_strideSummary.m_conversionFactor = 1.0 / (double)mainSummaryRatio; // required, because sumary is the everage of maxes
        }

        m_currentStride = 0;
        m_currentSummaryStride = 0;

        //debug
        //m_waveform->dump();
        //m_waveformSummary->dump();

    #ifdef TEST_HEAT_MAP
        test_heatMap = new QImage(256,256,QImage::Format_RGB32);
        test_heatMap->fill(0xFFFFFFFF);
    #endif
    }
    return !m_skipProcessing;
}

bool AnalyserWaveform::loadStored(TrackPointer tio) const {
    Waveform* waveform = tio->getWaveform();
    Waveform* waveformSummary = tio->getWaveformSummary();

    if (!waveform || !waveformSummary) {
        qWarning() << "AnalyserWaveform::loadStored - no waveform/waveform summary";
        return false;
    }

    int trackId = tio->getId();
    bool missingWaveform = waveform->getDataSize() == 0;
    bool missingWavesummary = waveformSummary->getDataSize() == 0;

    if (trackId != -1 && (missingWaveform || missingWavesummary)) {
        QList<AnalysisDao::AnalysisInfo> analyses =
                m_analysisDao->getAnalysesForTrack(trackId);

        QListIterator<AnalysisDao::AnalysisInfo> it(analyses);
        while (it.hasNext()) {
            const AnalysisDao::AnalysisInfo& analysis = it.next();

            if (analysis.type == AnalysisDao::TYPE_WAVEFORM &&
                    analysis.version == WaveformFactory::getPreferredWaveformVersion() &&
                    missingWaveform) {
                if (WaveformFactory::updateWaveformFromAnalysis(waveform, analysis)) {
                    missingWaveform = false;
                } else {
                    m_analysisDao->deleteAnalysis(analysis.analysisId);
                }
            } else if (analysis.type == AnalysisDao::TYPE_WAVESUMMARY &&
                    analysis.version == WaveformFactory::getPreferredWaveformSummaryVersion() &&
                    missingWavesummary) {
                if (WaveformFactory::updateWaveformFromAnalysis(waveformSummary, analysis)) {
                    tio->waveformSummaryNew();
                    missingWavesummary = false;
                } else {
                    m_analysisDao->deleteAnalysis(analysis.analysisId);
                }
            }
        }
    }

    // If we don't need to calculate the waveform/wavesummary, skip.
    if (!missingWaveform && !missingWavesummary) {
        qDebug() << "AnalyserWaveform::loadStored - Stored waveform loaded";
        return true;
    }
    return false;
}

void AnalyserWaveform::resetFilters(TrackPointer tio, int sampleRate) {
    Q_UNUSED(tio);
    Q_UNUSED(sampleRate);
    //TODO: (vRince) bind this with *actual* filter values ...
    // m_filter[Low] = new EngineFilterButterworth8(FILTER_LOWPASS, sampleRate, 200);
    // m_filter[Mid] = new EngineFilterButterworth8(FILTER_BANDPASS, sampleRate, 200, 2000);
    // m_filter[High] = new EngineFilterButterworth8(FILTER_HIGHPASS, sampleRate, 2000);
    m_filter[Low] = new EngineFilterIIR(bessel_lowpass4, 4);
    m_filter[Mid] = new EngineFilterIIR(bessel_bandpass, 8);
    m_filter[High] = new EngineFilterIIR(bessel_highpass4, 4);
}

void AnalyserWaveform::destroyFilters() {
    for( int i = 0; i < FilterCount; i++) {
        if( m_filter[i]) {
            delete m_filter[i];
            m_filter[i] = 0;
        }
    }
}

void AnalyserWaveform::process(const CSAMPLE *buffer, const int bufferLength) {
    if (m_skipProcessing || !m_waveform || !m_waveformSummary)
        return;

    //this should only append once if bufferLength is constant
    if( bufferLength > (int)m_buffers[0].size()) {
        m_buffers[Low].resize(bufferLength);
        m_buffers[Mid].resize(bufferLength);
        m_buffers[High].resize(bufferLength);
    }

    m_filter[Low]->process(buffer, &m_buffers[Low][0], bufferLength);
    m_filter[Mid]->process(buffer, &m_buffers[Mid][0], bufferLength);
    m_filter[High]->process(buffer, &m_buffers[High][0], bufferLength);


    for( int i = 0; i < bufferLength; i+=2) {
        // Take max value, not average of data
        CSAMPLE cover[2] = { fabs(buffer[i]), fabs(buffer[i+1]) };
        CSAMPLE clow[2] =  { fabs(m_buffers[ Low][i]), fabs(m_buffers[ Low][i+1]) };
        CSAMPLE cmid[2] =  { fabs(m_buffers[ Mid][i]), fabs(m_buffers[ Mid][i+1]) };
        CSAMPLE chigh[2] = { fabs(m_buffers[High][i]), fabs(m_buffers[High][i+1]) };

        // This is for if you want to experiment with averaging instead of
        // maxing.
        // m_stride.m_overallData[Right] += buffer[i]*buffer[i];
        // m_stride.m_overallData[ Left] += buffer[i+1]*buffer[i+1];
        // m_stride.m_filteredData[Right][ Low] += m_buffers[ Low][i  ]*m_buffers[ Low][i];
        // m_stride.m_filteredData[ Left][ Low] += m_buffers[ Low][i+1]*m_buffers[ Low][i+1];
        // m_stride.m_filteredData[Right][ Mid] += m_buffers[ Mid][i  ]*m_buffers[ Mid][i];
        // m_stride.m_filteredData[ Left][ Mid] += m_buffers[ Mid][i+1]*m_buffers[ Mid][i+1];
        // m_stride.m_filteredData[Right][High] += m_buffers[High][i  ]*m_buffers[High][i];
        // m_stride.m_filteredData[ Left][High] += m_buffers[High][i+1]*m_buffers[High][i+1];


        const double mainSummaryRatio = m_waveform->getVisualSampleRate() / m_waveformSummary->getVisualSampleRate();
        if (mainSummaryRatio < 1) {
            // Record the max across this stride.
             storeIfGreater(&m_strideSummary.m_overallData[Left], cover[Left]);
             storeIfGreater(&m_strideSummary.m_overallData[Right], cover[Right]);
             storeIfGreater(&m_strideSummary.m_filteredData[ Left][ Low], clow[Left]);
             storeIfGreater(&m_strideSummary.m_filteredData[Right][ Low], clow[Right]);
             storeIfGreater(&m_strideSummary.m_filteredData[ Left][ Mid], cmid[Left]);
             storeIfGreater(&m_strideSummary.m_filteredData[Right][ Mid], cmid[Right]);
             storeIfGreater(&m_strideSummary.m_filteredData[ Left][High], chigh[Left]);
             storeIfGreater(&m_strideSummary.m_filteredData[Right][High], chigh[Right]);

             if( m_strideSummary.m_position >= m_strideSummary.m_length) {
                 if (m_currentSummaryStride + ChannelCount > m_waveformSummaryDataSize) {
                     qWarning() << "AnalyserWaveform::process - currentStrideSummary >= waveform summary size";
                     return;
                 }

                 m_strideSummary.store(m_waveformSummaryData + m_currentSummaryStride);

                 // summary is an average of the maxes
                 m_stride.m_overallData[Left] += m_strideSummary.m_overallData[ Left];
                 m_stride.m_overallData[Right] += m_strideSummary.m_overallData[ Right];
                 m_stride.m_filteredData[ Left][ Low] += m_strideSummary.m_filteredData[ Left][ Low];
                 m_stride.m_filteredData[Right][ Low] += m_strideSummary.m_filteredData[Right][ Low];
                 m_stride.m_filteredData[ Left][ Mid] += m_strideSummary.m_filteredData[ Left][ Mid];
                 m_stride.m_filteredData[Right][ Mid] += m_strideSummary.m_filteredData[Right][ Mid];
                 m_stride.m_filteredData[ Left][High] += m_strideSummary.m_filteredData[ Left][High];
                 m_stride.m_filteredData[Right][High] += m_strideSummary.m_filteredData[Right][High];

                 if( m_stride.m_position >= m_stride.m_length) {
                     if (m_currentStride + ChannelCount > m_waveformDataSize) {
                         qWarning() << "AnalyserWaveform::process - current stride >= waveform size";
                         return;
                     }

                     m_stride.store(m_waveformData + m_currentStride);
                     m_stride.reset();
                     m_currentStride += 2;
                 }
                 m_stride.m_position += 2;
                 m_strideSummary.reset();
                 m_currentSummaryStride += 2;
             }
             m_strideSummary.m_position += 2;
        } else {
            // Record the max across this stride.
            storeIfGreater(&m_stride.m_overallData[Left], cover[Left]);
            storeIfGreater(&m_stride.m_overallData[Right], cover[Right]);
            storeIfGreater(&m_stride.m_filteredData[ Left][ Low], clow[Left]);
            storeIfGreater(&m_stride.m_filteredData[Right][ Low], clow[Right]);
            storeIfGreater(&m_stride.m_filteredData[ Left][ Mid], cmid[Left]);
            storeIfGreater(&m_stride.m_filteredData[Right][ Mid], cmid[Right]);
            storeIfGreater(&m_stride.m_filteredData[ Left][High], chigh[Left]);
            storeIfGreater(&m_stride.m_filteredData[Right][High], chigh[Right]);

            if( m_stride.m_position >= m_stride.m_length) {
                if (m_currentStride + ChannelCount > m_waveformDataSize) {
                    qWarning() << "AnalyserWaveform::process - currentStride >= waveform size";
                    return;
                }

                m_stride.store(m_waveformData + m_currentStride);

                // summary is an average of the maxes
                m_strideSummary.m_overallData[Left] += m_stride.m_overallData[ Left];
                m_strideSummary.m_overallData[Right] += m_stride.m_overallData[ Right];
                m_strideSummary.m_filteredData[ Left][ Low] += m_stride.m_filteredData[ Left][ Low];
                m_strideSummary.m_filteredData[Right][ Low] += m_stride.m_filteredData[Right][ Low];
                m_strideSummary.m_filteredData[ Left][ Mid] += m_stride.m_filteredData[ Left][ Mid];
                m_strideSummary.m_filteredData[Right][ Mid] += m_stride.m_filteredData[Right][ Mid];
                m_strideSummary.m_filteredData[ Left][High] += m_stride.m_filteredData[ Left][High];
                m_strideSummary.m_filteredData[Right][High] += m_stride.m_filteredData[Right][High];

                //NOTE: (vrince) test save main max in summary
                /*
                m_strideSummary.m_overallData[Right] = math_max(
                            m_strideSummary.m_overallData[Right],
                            m_stride.m_overallData[Right]);
                m_strideSummary.m_overallData[ Left] = math_max(
                            m_strideSummary.m_overallData[ Left],
                            m_stride.m_overallData[ Left]);
                m_strideSummary.m_filteredData[Right][ Low] = math_max(
                            m_strideSummary.m_filteredData[Right][ Low],
                            m_stride.m_filteredData[Right][ Low]);
                m_strideSummary.m_filteredData[ Left][ Low] = math_max(
                            m_strideSummary.m_filteredData[ Left][ Low],
                            m_stride.m_filteredData[ Left][ Low]);
                m_strideSummary.m_filteredData[Right][ Mid] = math_max(
                            m_strideSummary.m_filteredData[Right][ Mid],
                            m_stride.m_filteredData[Right][ Mid]);
                m_strideSummary.m_filteredData[ Left][ Mid] = math_max(
                            m_strideSummary.m_filteredData[ Left][ Mid],
                            m_stride.m_filteredData[ Left][ Mid]);
                m_strideSummary.m_filteredData[Right][High] = math_max(
                            m_strideSummary.m_filteredData[Right][High],
                            m_stride.m_filteredData[Right][High]);
                m_strideSummary.m_filteredData[ Left][High] = math_max(
                            m_strideSummary.m_filteredData[ Left][High],
                            m_stride.m_filteredData[ Left][High]);
                            */


                if( m_strideSummary.m_position >= m_strideSummary.m_length) {
                    if (m_currentSummaryStride + ChannelCount > m_waveformSummaryDataSize) {
                        qWarning() << "AnalyserWaveform::process - current summary stride >= waveform summary size";
                        return;
                    }

    #ifdef TEST_HEAT_MAP
                    QPointF point(float(m_strideSummary.m_filteredData[Right][High]),
                                  float(m_strideSummary.m_filteredData[Right][ Mid]));

                    float norm = sqrt(point.x()*point.x() + point.y()*point.y());
                    point /= norm;

                    point *= m_strideSummary.m_filteredData[Right][ Low];
                    test_heatMap->setPixel(point.toPoint(),0xFF0000FF);
    #endif

                    m_strideSummary.store(m_waveformSummaryData + m_currentSummaryStride);
                    m_strideSummary.reset();


    /*                qDebug() << "m_strideSummary"
                             << (m_waveformSummaryData + m_currentSummaryStride)->filtered.all
                             << (m_waveformSummaryData + m_currentSummaryStride)->filtered.low
                             << (m_waveformSummaryData + m_currentSummaryStride)->filtered.mid
                             << (m_waveformSummaryData + m_currentSummaryStride)->filtered.high;*/


                    m_currentSummaryStride += 2;
                }
                m_strideSummary.m_position += 2;
                m_stride.reset();
                m_currentStride += 2;
            }
            m_stride.m_position += 2;
        }
    }

    m_waveform->setCompletion(m_currentStride);
    m_waveformSummary->setCompletion(m_currentSummaryStride);

    //qDebug() << "AnalyserWaveform::process - m_waveform->getCompletion()" << m_waveform->getCompletion() << "off" << m_waveform->getDataSize();
    //qDebug() << "AnalyserWaveform::process - m_waveformSummary->getCompletion()" << m_waveformSummary->getCompletion() << "off" << m_waveformSummary->getDataSize();
}

void AnalyserWaveform::cleanup(TrackPointer tio) {
    Q_UNUSED(tio);
    if (m_skipProcessing) {
        return;
    }

    if (m_waveform) {
        m_waveform->reset();
    }

    if (m_waveformSummary) {
        m_waveformSummary->reset();
    }
}

void AnalyserWaveform::finalise(TrackPointer tio) {
    if (m_skipProcessing || m_waveform == NULL || m_waveformSummary == NULL) {
        return;
    }

    QMutexLocker waveformLocker(m_waveform->getMutex());
    // Force completion to waveform size
    m_waveform->setCompletion(m_waveform->getDataSize());
    m_waveform->setVersion(WaveformFactory::getPreferredWaveformVersion());
    m_waveform->setDescription(WaveformFactory::getPreferredWaveformDescription());

    QMutexLocker waveformSummaryLocker(m_waveformSummary->getMutex());
    // Force completion to waveform size
    m_waveformSummary->setCompletion(m_waveformSummary->getDataSize());
    m_waveformSummary->setVersion(WaveformFactory::getPreferredWaveformSummaryVersion());
    m_waveformSummary->setDescription(WaveformFactory::getPreferredWaveformSummaryDescription());


#ifdef TEST_HEAT_MAP
    test_heatMap->save("heatMap.png");
#endif

    qDebug() << "Waveform generation for track" << tio->getId() << "done"
             << m_timer->elapsed()/1000.0 << "s";
}

void AnalyserWaveform::storeIfGreater(float* pDest, float source) {
    if (*pDest < source) {
        *pDest = source;
    }
}
