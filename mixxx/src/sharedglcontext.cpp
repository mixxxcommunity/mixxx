#include <QGLContext>
#include <QGLFormat>
#include "sharedglcontext.h"

/** Singleton wrapper around QGLContext */

QGLContext* SharedGLContext::s_pSharedGLContext = (QGLContext*)NULL;

QGLContext* SharedGLContext::getContext() {
    if (s_pSharedGLContext == (QGLContext*)NULL) {
        // QGLFormat::setDoubleBuffer()
        s_pSharedGLContext = new QGLContext(QGLFormat::defaultFormat());
        // Note: We have no valid painting device yet
    }
    // http://lists.trolltech.com/qt-interest/2006-04/msg00313.html

    QGLContext* ctxt = new QGLContext(QGLFormat::defaultFormat());
    // note: We have still no valid painting device
    // create() is called from QGLWidget::setContext later
    //ctxt->create(s_pSharedGLContext);
    return ctxt;
}
