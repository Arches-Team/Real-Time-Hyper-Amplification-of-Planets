#include "mainwindow.h"

//#include "tool.h"
//#include "simpleterraincontroler.h"

#include <iostream>
#include <QtWidgets>


MainWindow* MainWindow::m_instance = nullptr;
MainWindow * MainWindow::instance()
{
	if (m_instance == nullptr)
		m_instance = new MainWindow;
	return m_instance;
}

void MainWindow::release()
{
	delete m_instance;
}

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	setMinimumSize(1280, 768);
	mainPanel = new QWidget;
	setCentralWidget(mainPanel);

	controlPanel = new QStackedWidget;
	controlPanel->setFrameStyle(QFrame::Panel | QFrame::Raised);
	controlPanel->setMinimumWidth(420);

	glView = new GLWidget;

	planetModule = new PlanetModule(glView);
	glView->setCurrentModule(planetModule);
	planetModuleControler = new PlanetModuleControler(planetModule);
	controlPanel->addWidget(planetModuleControler);

	controlPanel->setCurrentIndex(0);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	mainLayout->addWidget(glView);
	mainLayout->addWidget(controlPanel, 0, Qt::AlignRight);
	mainPanel->setLayout(mainLayout);

	createActions();
	createMenus();
	createStatusBar();
}

MainWindow::~MainWindow()
{
}

void MainWindow::toggleFullscreen()
{
	m_fullscreen = !m_fullscreen;
	if (m_fullscreen)
	{
		m_default_glsize = glView->size();
		int w = mainPanel->size().width();
		int h = mainPanel->size().height();
		glView->resize(QSize(w, h));//FixedSize(w, h);
		
		controlPanel->hide();
	}
	else {
		glView->resize(m_default_glsize);//FixedSize(w, h);
		controlPanel->show();
	}
}

void MainWindow::createActions()
{
	quitAct = new QAction(tr("Q&uit"), this);
	quitAct->setShortcuts(QKeySequence::Quit);
	quitAct->setStatusTip(tr("Quit the application"));
	connect(quitAct, SIGNAL(triggered()), this, SLOT(close()));
}

void MainWindow::createMenus()
{
	fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addSeparator();
	fileMenu->addAction(quitAct);

	menuBar()->addSeparator();
}

void MainWindow::createStatusBar()
{
	//statusBar()->showMessage(tr("Ready"));
	
	fpsLabel = new QLabel("Framerate    Hz");
	//fpsLabel->setAlignment(Qt::AlignRight);
	
	msgLabel = new QLabel("Ready");
	//msgLabel->setAlignment(Qt::AlignLeft);

	QWidget * container = new QWidget();	
	QHBoxLayout *box = new QHBoxLayout;
	box->addWidget(msgLabel);
	box->addStretch();	
	box->addSpacing(1600);
	box->addWidget(fpsLabel);
	container->setLayout(box);

	statusBar()->addPermanentWidget(container);
	//statusBar()->addPermanentWidget(fpsLabel);
}

void MainWindow::showStatusMsg(QString & msg) {
	//statusBar()->showMessage(msg);
	msgLabel->setText(msg);
}

void MainWindow::showFramerate(float framerate)
{
	QString fps;
	fps.setNum(static_cast<int>(framerate));
	QString fpsDisplay("Framerate ");
	fpsDisplay = fpsDisplay + fps + QString(" Hz");
	fpsLabel->setText(fpsDisplay);
}

void MainWindow::showWaitCursor(bool w)
{
	if (w)
		setCursor(Qt::WaitCursor);
	else setCursor(Qt::ArrowCursor);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	if (true) {
		event->accept();
	}
	else {
		event->ignore();
	}
}

void MainWindow::about() {}

