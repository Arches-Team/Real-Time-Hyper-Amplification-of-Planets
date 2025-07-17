#ifndef GL_WIDGET_HEADER
#define GL_WIDGET_HEADER

#include "glversion.h"
#include "appmodule.h"

#include <QOpenGLWidget>
#include <QtCore/qelapsedtimer.h>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>

#include <vector>


class GLWidget : public QOpenGLWidget, protected Q_OPENGL_FUNCS
{
	Q_OBJECT

public:
	GLWidget(QWidget *parent = 0)
		: QOpenGLWidget(parent)
		, modules(1)
		, currentModule(nullptr)
		, m_frame(0), frameCount(0), totalTime(0.0f), frame60Time(0.0f)
	{
		setFocusPolicy(Qt::StrongFocus);//for key events
		setUpdateBehavior(QOpenGLWidget::UpdateBehavior::PartialUpdate);//don't invalidate framebuffers
	}
	~GLWidget();

	/**
	* @brief Changes the current application module.
	* By calling this function AppModule::init() is called.
	* This object keeps track of added module, so that each respective AppModule::cleanup() is called when terminating the application.
	*/
	void setCurrentModule(AppModule * module = nullptr);
	void addModule(AppModule * module);

	QSize minimumSizeHint() const Q_DECL_OVERRIDE;
	QSize sizeHint() const Q_DECL_OVERRIDE;

	/** @brief Returns the total time (in seconds) since the start of the application. */
	float getTimeSeconds();


protected:
	virtual void initializeGL() Q_DECL_OVERRIDE;
	virtual void paintGL() Q_DECL_OVERRIDE;
	virtual void resizeGL(int width, int height) Q_DECL_OVERRIDE;
	void cleanup();
	virtual void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	virtual void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	virtual void wheelEvent(QWheelEvent * event) Q_DECL_OVERRIDE;
	virtual void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
	virtual void keyReleaseEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

private:
	std::vector<AppModule*> modules;
	AppModule * currentModule;
	QElapsedTimer timer;
	QPoint mouse_lastPos;

	int m_frame;
	int frameCount;
	float totalTime, frame60Time;
};

#endif //GL_WIDGET_HEADER
