#pragma once

#include "PlanetModule.h"

#include <QGroupBox>
#include <qslider.h>
#include <qlabel.h>
#include <qradiobutton.h>
#include <qbuttongroup.h>



class PlanetModuleControler : public QGroupBox
{
	Q_OBJECT

public:
	PlanetModuleControler(PlanetModule * module)
		: QGroupBox("On-the-fly Planet Generation")
		, module(module)
	{
		createUI();
	}

private:
	void createUI();

private slots:
	void update();
	void loadPlanetClicked(bool checked);
	void loadMapsClicked(bool checked);
	void loadCameraClicked(bool checked);
	void saveCameraClicked(bool checked);

	/*void activeSimulationClicked(bool checked);
	void colorsModeClicked(int button);
	void renderFramesStateChanged(int state);
	void showAltitudeLayerStateChanged(int state);
	void renderTracersStateChanged(int state);
	void renderCloudsStateChanged(int state);
	*/
	void cameraModeClicked(int button);
	void sunDirectionUpdate(int sliderVal);
	//void renderVideoClicked(bool checked);
	void exportVideoClicked(bool checked);
	
private:
	PlanetModule * module = nullptr;
	
	QRadioButton * radioVelocity = nullptr;
	QRadioButton * radioPressure = nullptr;
	QRadioButton * radioTemperature = nullptr;
	QRadioButton * radioDensity = nullptr;	
	QRadioButton * radioNone = nullptr;
	QButtonGroup * colorsModeGroup = nullptr;
	QLabel * simulationTimeLabel = nullptr;

	QRadioButton * radioOrbit = nullptr;
	QRadioButton * radioFreefly = nullptr;
	QButtonGroup * cameraModeGroup = nullptr;

	QLabel * renderingStageAltitudeLabel = nullptr;	

	bool m_simul_active = false;
};
