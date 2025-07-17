#include "glversion.h"
#include "MainWindow.h"

#include <QtWidgets/QApplication>
#include <QSurfaceFormat>

#define WIN32_MEAN_AND_LEAN
#include <Windows.h>
#include <Wincon.h>

#define DEBUG_CONSOLE_OUT


int main(int argc, char *argv[])
{
	QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QApplication app(argc, argv);
	app.setOrganizationName("LIRIS - ORIGAMI");
	app.setApplicationName("RTHAPlanets");

	QSurfaceFormat format;
	//format.setSamples(4);
	format.setRenderableType(QSurfaceFormat::RenderableType::OpenGL);
	format.setSamples(1);
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setVersion(APP_GL_VERSION_MAJOR, APP_GL_VERSION_MINOR);
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setSwapInterval(1);//1 = vsync
	//format.setSwapInterval(0);//1 = vsync
	QSurfaceFormat::setDefaultFormat(format);

#ifdef DEBUG_CONSOLE_OUT
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
#endif

	MainWindow::instance()->showMaximized();
	int result = app.exec();
	MainWindow::release();

	return result;
}
