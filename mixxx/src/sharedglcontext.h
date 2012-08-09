#ifndef SHAREDGLCONTEXT_H_
#define SHAREDGLCONTEXT_H_

class QGLWidget;

class SharedGLContext 
{
    public:
        static const QGLWidget* getShareWidget();
    private:
        SharedGLContext() { };
        static QGLWidget* s_pShareWidget;
};

#endif //SHAREDGLCONTEXT_H_
