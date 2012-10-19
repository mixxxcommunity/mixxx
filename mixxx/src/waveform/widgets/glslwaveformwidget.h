#ifndef GLWAVEFORMWIDGETSHADER_H
#define GLWAVEFORMWIDGETSHADER_H

#include <QGLWidget>

#include "waveformwidgetabstract.h"

class GLSLWaveformRendererSignal;

class GLSLWaveformWidget : public QGLWidget, public WaveformWidgetAbstract {
    Q_OBJECT
  public:
    GLSLWaveformWidget(const char* group, QWidget* parent);
    virtual ~GLSLWaveformWidget();

    virtual WaveformWidgetType::Type getType() const { return WaveformWidgetType::GLWaveform;}

    static inline QString getWaveformWidgetName() { return tr("Filtered") + " - " + tr("experimental");}
    static inline bool useOpenGl() { return true;}
    static inline bool useOpenGLShaders() { return true;}

    virtual void resize( int width, int height);

  protected:
    virtual void castToQWidget();
    virtual void paintEvent(QPaintEvent* event);
    virtual void mouseDoubleClickEvent(QMouseEvent *);
    virtual void postRender();
    virtual void render();

  private:
    GLSLWaveformRendererSignal* signalRenderer_;

    friend class WaveformWidgetFactory;
};

#endif // GLWAVEFORMWIDGETSHADER_H
