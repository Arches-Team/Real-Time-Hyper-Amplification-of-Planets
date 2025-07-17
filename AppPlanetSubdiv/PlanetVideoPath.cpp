#include "PlanetVideoPath.h"

#include "Common.h"

#include <cmath>



void PlanetVideoPath::buildPath()
{
	double cam_alt_0 = glm::length(start_camera.getPosition()) - planetRadius;
	double cam_alt_1 = glm::length(end_camera.getPosition()) - planetRadius;

	lerpType = LerpType::LINEAR;

	//if (cam_alt_0 < 100.0 / RENDER_SCALE && cam_alt_1 > 1000.0 / RENDER_SCALE)
	//	lerpType = LerpType::SPARSE_END;
	//if (cam_alt_0 > 1000.0 / RENDER_SCALE && cam_alt_1 < 100.0 / RENDER_SCALE)
	//	lerpType = LerpType::DENSE_END;

	//
	curve = new tool::NURBSCurve(3);
	int num_ctrl = cameras_positions.size();
	control_points = new glm::dvec4[num_ctrl];
	for (int i = 0; i < num_ctrl; ++i)
	{
		const glm::dvec3 & c = cameras_positions[i];
		control_points[i].x = c[0];
		control_points[i].y = c[1];
		control_points[i].z = c[2];
		control_points[i].w = 1.0;
	}
	curve->setControlPoints(num_ctrl, control_points);
	curve->buildUniformNodalSequence();

	path_built = true;
}


tool::FreeFlyCameraDouble PlanetVideoPath::getCamera(double t)
{
	if (!path_built)
	{
		buildPath();
	}

	double u = t;
	if (lerpType == LerpType::SPARSE_END)
		u = std::pow(t, 3.0);
	if (lerpType == LerpType::DENSE_END)
		u = std::pow(t, 0.33);

	glm::dvec4 p = curve->getCurvePoint(u);
	glm::dvec3 cameraPosition(p.x, p.y, p.z);

	tool::FreeFlyCameraDouble camera;
	camera.setPosition(cameraPosition);
	camera.setForwardDirection(glm::normalize(start_camera.lookDir * (1.0 - u) + u * end_camera.lookDir));
	camera.setUpDirection(glm::normalize(start_camera.upDir * (1.0 - u) + u * end_camera.upDir));

	return camera;
}

