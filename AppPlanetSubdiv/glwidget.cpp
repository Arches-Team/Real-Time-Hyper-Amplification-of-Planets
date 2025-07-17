#include "glwidget.h"
#include "mainwindow.h"
#include "appmodule.h"



GLWidget::~GLWidget()
{
	cleanup();
}

void GLWidget::cleanup()
{
	makeCurrent();
	for (std::vector<AppModule*>::const_iterator it = modules.cbegin(); it != modules.cend(); ++it)
		if (*it != nullptr) {
			(*it)->cleanup();
		}
	doneCurrent();
}

QSize GLWidget::minimumSizeHint() const
{
	return QSize(1024, 768);
}

QSize GLWidget::sizeHint() const
{
	return QSize(1360, 900);
}

void GLWidget::resizeGL(int w, int h)
{
	const qreal retinaScale = devicePixelRatio();
	for (std::vector<AppModule*>::const_iterator it = modules.cbegin(); it != modules.cend(); ++it)
		if (*it != nullptr)
			(*it)->onResize((int)(w * retinaScale), (int)(h * retinaScale));
}

/*void GLWidget::setCurrentModule(AppModule * module)
{
	currentModule = module;
	for (auto it = modules.cbegin(); it != modules.cend(); ++it)
		if ((*it) == module)
			return;
	modules.push_back(module);
}*/
void GLWidget::setCurrentModule(AppModule * module)
{
	currentModule = module;
	addModule(module);
}

void GLWidget::addModule(AppModule * module)
{
	for (auto it = modules.cbegin(); it != modules.cend(); ++it)
		if ((*it) == module)
			return;
	modules.push_back(module);
}

void GLWidget::initializeGL()
{
	connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &GLWidget::cleanup);
	initializeOpenGLFunctions();
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	timer.start();
}

void GLWidget::paintGL()
{
	const qreal retinaScale = devicePixelRatio();
	//glViewport(0, 0, width() * retinaScale, height() * retinaScale);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (currentModule != nullptr && currentModule->needInit())
		currentModule->init();

	if (currentModule != nullptr)
		currentModule->render();

	++frameCount;
	float elapsed = static_cast<float>(timer.restart() / 1000.0);
	totalTime += elapsed;
	frame60Time += elapsed;
	if (frameCount == 60) {
		float frameRate = 60.0f / frame60Time;
		frameCount = 0;
		frame60Time = 0.0f;
		MainWindow::instance()->showFramerate(frameRate);
	}
	else
		++m_frame;

	//
	update(); //re-poster un update event sur le thread du GUI Qt

	if (currentModule != nullptr)
		currentModule->onPostRender();
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
	mouse_lastPos = event->pos();
	if (currentModule == nullptr)
		return;
	currentModule->onMousePressed(event->x(), event->y(), event->buttons() & Qt::LeftButton, event->buttons() & Qt::RightButton);
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
	int dx = event->x() - mouse_lastPos.x();
	int dy = event->y() - mouse_lastPos.y();
	mouse_lastPos = event->pos();

	if (currentModule == nullptr)
		return;
	currentModule->onMouseMoved(dx, dy, event->buttons() & Qt::LeftButton, event->buttons() & Qt::RightButton);
}

void GLWidget::wheelEvent(QWheelEvent * event)
{
	if (currentModule != nullptr)
		currentModule->onMouseWheel(event->angleDelta().y() > 0);
	event->accept();
}

void GLWidget::keyPressEvent(QKeyEvent * event)
{
	if (currentModule != nullptr && currentModule->onKeyPress(event))
		return;
	else
		QWidget::keyPressEvent(event);
}

void GLWidget::keyReleaseEvent(QKeyEvent * event)
{
	if (currentModule != nullptr && currentModule->onKeyRelease(event))
		return;
	QWidget::keyReleaseEvent(event);
}

float GLWidget::getTimeSeconds()
{
	return totalTime;
}
