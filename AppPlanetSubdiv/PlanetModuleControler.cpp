#include "PlanetModuleControler.h"
#include "MainWindow.h"

#include "tool.h"

#include <QtWidgets>
#include <QTimer>


void PlanetModuleControler::createUI()
{
	QPushButton *loadPlanetButton = new QPushButton("Load Tectonic Planet");
	connect(loadPlanetButton, SIGNAL(clicked(bool)), this, SLOT(loadPlanetClicked(bool)));

	QPushButton *loadMapsButton = new QPushButton("Load Planet From Maps");
	connect(loadMapsButton, SIGNAL(clicked(bool)), this, SLOT(loadMapsClicked(bool)));

	QPushButton *loadCameraButton = new QPushButton("Load Freefly Camera");
	connect(loadCameraButton, SIGNAL(clicked(bool)), this, SLOT(loadCameraClicked(bool)));

	QPushButton *saveCameraButton = new QPushButton("Save Freefly Camera");
	connect(saveCameraButton, SIGNAL(clicked(bool)), this, SLOT(saveCameraClicked(bool)));

	//QPushButton *exportVideoButton = new QPushButton("Video with Camera Path");
	//connect(exportVideoButton, SIGNAL(clicked(bool)), this, SLOT(exportVideoClicked(bool)));
	
	/*QPushButton *activeSimulationButton = new QPushButton("Start / Stop simulation");
	connect(activeSimulationButton, SIGNAL(clicked(bool)), this, SLOT(activeSimulationClicked(bool)));

	QCheckBox * renderFramesCheckbox = new QCheckBox();
	renderFramesCheckbox->setChecked(false);
	connect(renderFramesCheckbox, SIGNAL(stateChanged(int)), this, SLOT(renderFramesStateChanged(int)));

	QGroupBox * colorsModeGroupBox = new QGroupBox(tr("False Colors"));
	colorsModeGroupBox->setFlat(true);
	radioVelocity = new QRadioButton(tr("velocity"));
	radioPressure = new QRadioButton(tr("pressure"));
	radioPressure->setChecked(true);
	radioTemperature = new QRadioButton(tr("temperature"));
	radioDensity = new QRadioButton(tr("density"));
	radioNone = new QRadioButton(tr("none"));
	QHBoxLayout *hbox_colors = new QHBoxLayout;
	hbox_colors->addWidget(radioVelocity);
	hbox_colors->addWidget(radioPressure);
	hbox_colors->addWidget(radioTemperature);
	hbox_colors->addWidget(radioDensity);
	hbox_colors->addWidget(radioNone);
	colorsModeGroupBox->setLayout(hbox_colors);
	colorsModeGroup = new QButtonGroup(this);
	colorsModeGroup->addButton(radioVelocity);
	colorsModeGroup->setId(radioVelocity, 0);
	colorsModeGroup->addButton(radioPressure);
	colorsModeGroup->setId(radioPressure, 1);
	colorsModeGroup->addButton(radioTemperature);
	colorsModeGroup->setId(radioTemperature, 2);
	colorsModeGroup->addButton(radioDensity);
	colorsModeGroup->setId(radioDensity, 3);
	colorsModeGroup->addButton(radioNone);
	colorsModeGroup->setId(radioNone, 4);
	colorsModeGroup->setExclusive(true);
	connect(colorsModeGroup, SIGNAL(buttonClicked(int)), this, SLOT(colorsModeClicked(int)));

	QCheckBox * renderTracersCheckbox = new QCheckBox();
	renderTracersCheckbox->setChecked(false);
	connect(renderTracersCheckbox, SIGNAL(stateChanged(int)), this, SLOT(renderTracersStateChanged(int)));

	QCheckBox * renderCloudsCheckbox = new QCheckBox();
	renderCloudsCheckbox->setChecked(false);
	connect(renderCloudsCheckbox, SIGNAL(stateChanged(int)), this, SLOT(renderCloudsStateChanged(int)));

	QCheckBox * showAltitudeLayerCheckbox = new QCheckBox();
	showAltitudeLayerCheckbox->setChecked(false);
	connect(showAltitudeLayerCheckbox, SIGNAL(stateChanged(int)), this, SLOT(showAltitudeLayerStateChanged(int)));

	simulationTimeLabel = new QLabel("");
	*/
	///////////////////////////////////////////////////////

	QGroupBox * cameraModeGroupBox = new QGroupBox(tr(" "));
	cameraModeGroupBox->setFlat(true);
	radioOrbit = new QRadioButton(tr("Orbit"));
	radioFreefly = new QRadioButton(tr("Free fly"));
	radioFreefly->setChecked(true);
	QHBoxLayout *hbox2 = new QHBoxLayout;
	hbox2->addWidget(radioOrbit);
	hbox2->addWidget(radioFreefly);
	cameraModeGroupBox->setLayout(hbox2);
	cameraModeGroup = new QButtonGroup(this);
	cameraModeGroup->addButton(radioOrbit);
	cameraModeGroup->setId(radioOrbit, 0);
	cameraModeGroup->addButton(radioFreefly);
	cameraModeGroup->setId(radioFreefly, 1);
	cameraModeGroup->setExclusive(true);
	connect(cameraModeGroup, SIGNAL(buttonClicked(int)), this, SLOT(cameraModeClicked(int)));

	QSlider * sunDirectionSlider = new QSlider(Qt::Horizontal);
	sunDirectionSlider->setMinimum(0);
	sunDirectionSlider->setMaximum(359);
	sunDirectionSlider->setSliderPosition(0);
	connect(sunDirectionSlider, SIGNAL(valueChanged(int)), this, SLOT(sunDirectionUpdate(int)));

	renderingStageAltitudeLabel = new QLabel("");

	QWidget * initContainer = new QWidget();
	initContainer->setAutoFillBackground(true);
	QPalette initPal;
	initPal.setColor(QPalette::Window, Qt::white);
	initContainer->setPalette(initPal);

	QFormLayout *initForm = new QFormLayout;
	initForm->addRow(new QLabel(""), new QLabel(""));
	initForm->addRow(new QLabel(" "), loadPlanetButton);
	initForm->addRow(new QLabel(" "), loadMapsButton);
	initForm->addRow(new QLabel(" "), new QLabel(""));
	
	initForm->addRow(new QLabel("Camera mode: "), cameraModeGroupBox);
	initForm->addRow(new QLabel(""), loadCameraButton);
	initForm->addRow(new QLabel(""), saveCameraButton);
	initForm->addRow(new QLabel("Sun direction: "), sunDirectionSlider);
	initForm->addRow(new QLabel(""), new QLabel(""));
	initForm->addRow(new QLabel("altitude: "), renderingStageAltitudeLabel);
	initForm->addRow(new QLabel(""), new QLabel(""));
	initForm->addRow(new QLabel(""), new QLabel(""));
	initForm->addRow(new QLabel(""), new QLabel(""));
	initForm->addRow(new QLabel("Keyboard Export Commands:"), new QLabel(""));
	//initForm->addRow(new QLabel(""), exportVideoButton);
	initForm->addRow(new QLabel("    C: "), new QLabel("Take a screenshot"));
	initForm->addRow(new QLabel(""), new QLabel(""));
	initForm->addRow(new QLabel("    F9: "), new QLabel("Start / stop LIVE recording (video frames)"));
	initForm->addRow(new QLabel(""), new QLabel(""));
	initForm->addRow(new QLabel("    F1: "), new QLabel("Camera Path -> start new segment"));
	initForm->addRow(new QLabel("    F2: "), new QLabel("Camera Path -> add control point to current segment"));
	initForm->addRow(new QLabel("    F4: "), new QLabel("Camera Path -> save all video frames for current camera path"));
	initContainer->setLayout(initForm);

	QVBoxLayout * globalLayout = new QVBoxLayout;
	globalLayout->addWidget(initContainer);
	setLayout(globalLayout);	

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(update()));
	timer->start(500);
}

void PlanetModuleControler::update()
{
	/*
	double simtime = module->getCurrentSimulationTime();
	double th = std::floor(simtime / 3600.0);
	double ts = simtime - th * 3600.0;
	double tm = std::floor(ts / 60.0);
	ts = ts - tm * 60.0;

	int frame = module->getFrameNumber();
	QString tim = QString("%1").arg((int)th) + QString(":%1").arg((int)tm) + QString(":%1 (h:m:s)").arg((int)ts) + QString(", frame %1").arg(frame);
	simulationTimeLabel->setText(tim);
	*/
	
	double a = module->getCurrentAltitude();
	if (a >= 1.0)
		renderingStageAltitudeLabel->setText(QString("%1 km").arg(a));
	else renderingStageAltitudeLabel->setText(QString("%1 m").arg(a*1000.0));	
}


void PlanetModuleControler::loadPlanetClicked(bool checked)
{
	//MainWindow::instance()->showWaitCursor(true);
	
	QFileDialog dialog(this);
	QStringList fileList;
	if (dialog.exec())
		fileList = dialog.selectedFiles();
	if (fileList.isEmpty())
		return;

	QString filename = fileList.at(0);
	module->loadPlanet(filename.toStdString());
	
	//module->loadPlanet("");
	//MainWindow::instance()->showWaitCursor(false);
}

void PlanetModuleControler::loadMapsClicked(bool checked)
{
	//QString dirname("../assets/planets/maps/Earth/");
	module->loadEarth();
}

void PlanetModuleControler::loadCameraClicked(bool checked)
{
	module->loadFreeflyCamera();
}

void PlanetModuleControler::saveCameraClicked(bool checked)
{
	module->saveFreeflyCamera();
}


void PlanetModuleControler::cameraModeClicked(int button)
{
	switch (button)
	{
	case 0:
		module->setCameraModeOrbit(true);
		break;
	case 1:
		module->setCameraModeOrbit(false);
		break;
	default:break;
	}
}

void PlanetModuleControler::sunDirectionUpdate(int sliderVal)
{
	double angle = (double)sliderVal * 3.1415926535897932384626433832795 / 180.0;
	math::dvec3 sunDirection(std::cos(angle), std::sin(angle), 0.0);
	module->setSunDirection(sunDirection);
}

void PlanetModuleControler::exportVideoClicked(bool checked)
{
	module->exportVideo();
}

/*void PlanetModuleControler::renderVideoClicked(bool checked)
{
	module->renderVideo();
}
*/