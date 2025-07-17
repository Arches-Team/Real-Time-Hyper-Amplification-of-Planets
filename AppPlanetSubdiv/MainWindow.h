#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "glwidget.h"
#include "PlanetModule.h"
#include "PlanetModuleControler.h"

#include <QMainWindow>
#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QMenu>
#include <QAction>



class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	static MainWindow* instance();// get singleton instance
	static void release();
	~MainWindow();

	void toggleFullscreen();
	void showWaitCursor(bool w);
	void showStatusMsg(QString & msg);
	void showFramerate(float framerate);
	void requestExit();

protected:
  void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

private:
	MainWindow(QWidget *parent = 0);// private Ctor
	void createActions();
	void createMenus();
	void createStatusBar();

	private slots:
	void about();

private:
	static MainWindow * m_instance;// singleton

	PlanetModule * planetModule = nullptr;
	PlanetModuleControler * planetModuleControler = nullptr;

	QWidget * mainPanel;
	GLWidget *glView;
	QStackedWidget * controlPanel;
	QLabel *fpsLabel;
	QLabel *msgLabel;

	QMenu * fileMenu;
	QMenu * moduleMenu;
	QMenu * helpMenu;
	QAction * quitAct;
	QAction * aboutAct;
	QAction * aboutQtAct;
	QAction * funcCloudsAct, *animCloudsAct;

	QSize m_default_glsize;
	bool m_fullscreen = false;
};

#endif // MAINWINDOW_H
