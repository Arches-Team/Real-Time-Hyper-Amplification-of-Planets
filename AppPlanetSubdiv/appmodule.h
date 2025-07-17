#ifndef APP_MODULE_H
#define APP_MODULE_H

#include <QObject>
#include <QKeyEVent>

//Fwrdcl:
class GLWidget;


/**
* @brief Abstract class (application module)
*/
class AppModule : public QObject
{
	Q_OBJECT

public:
	AppModule(GLWidget * host)
		: QObject()
		, widget(host)
		, viewportWidth(0)
		, viewportHeight(0)
		, init_done(false)
	{}
	virtual ~AppModule() {}

	virtual void init();
	virtual void render() = 0;
	virtual void onPostRender();
	virtual void cleanup();
	virtual void onResize(int w, int h);
	virtual void onMousePressed(int x, int y, bool leftButton, bool rightButton) = 0;
	virtual void onMouseMoved(int dx, int dy, bool leftButton, bool rightButton) = 0;
	virtual void onMouseWheel(bool forward) = 0;
	virtual bool onKeyPress(QKeyEvent * event) = 0;
	virtual bool onKeyRelease(QKeyEvent * event) = 0;

	bool needInit() {
		return !init_done;
	}

protected:
	GLWidget * widget;
	int viewportWidth, viewportHeight;
	bool init_done;
};

#endif // APP_MODULE_H

