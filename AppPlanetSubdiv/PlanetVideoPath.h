#pragma once

#include "RenderablePlanet.h"

#include "tool.h"
#include "FreeFlyCamera.h"
#include "NURBS.h"

#include <vector>



class PlanetVideoPath
{
public:
	PlanetVideoPath(double planetRadius)
		: planetRadius(planetRadius)
	{}
	~PlanetVideoPath()
	{
		delete curve;
		delete[] control_points;
		control_points = nullptr;
		curve = nullptr;
		cameras_positions.clear();
		path_built = false;
	}

	void buildPath();

	void setStartSun(const glm::dvec3 & v) { start_sun_dir = v; }
	
	void setEndSun(const glm::dvec3 & v) { end_sun_dir = v; }

	void setStartCamera(const tool::FreeFlyCameraDouble & cam) { start_camera = cam; }

	void setEndCamera(const tool::FreeFlyCameraDouble & cam) { end_camera = cam; }

	void addCameraPosition(const glm::dvec3 & campos) { cameras_positions.push_back(campos); }


	/// @param t in [0, 1]
	tool::FreeFlyCameraDouble getCamera(double t);	

	glm::dvec3 getSunDir(double t){	return glm::normalize((1.0 - t)*start_sun_dir + t * end_sun_dir); }


private:

	enum class LerpType {
		LINEAR,
		DENSE_END,
		SPARSE_END
	};

	tool::NURBSCurve * curve = nullptr;
	glm::dvec4 * control_points = nullptr;
	std::vector<glm::dvec3> cameras_positions;
	tool::FreeFlyCameraDouble start_camera, end_camera;
	glm::dvec3 start_sun_dir;
	glm::dvec3 end_sun_dir;
	double planetRadius;
	LerpType lerpType;
	bool path_built = false;
};
