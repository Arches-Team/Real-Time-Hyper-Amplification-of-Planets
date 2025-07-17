#pragma once

#include "tool.h"



namespace tool
{

	struct FreeFlyCameraDouble
	{
		glm::dmat4 projection;
		glm::dvec3 eye;
		glm::dvec3 lookDir;
		glm::dvec3 upDir;

	public:

		inline FreeFlyCameraDouble()
			: eye(1.0, 0.0, 0.0), lookDir(-1.0, 0.0, 0.0), upDir(0.0, 0.0, 1.0)
		{
		}
		inline ~FreeFlyCameraDouble()
		{
		}

		inline FreeFlyCameraDouble& operator=(FreeFlyCameraDouble const& c) {
			eye = c.eye;
			lookDir = c.lookDir;
			upDir = c.upDir;
			projection = c.projection;
			return *this;
		}

		/// @return the OpenGL compatible view matrix associated with the camera's current state. 
		inline glm::dmat4 getViewMatrix()
		{
		
			return glm::lookAt(eye, eye + lookDir, upDir);
		}

		inline glm::dmat4 perspective(double verticalFOVDegrees, double viewRatio, double nearplane, double farplane)
		{
			projection = glm::perspective(glm::radians(verticalFOVDegrees), viewRatio, nearplane, farplane);
			return projection;
		}

		inline void translate(double step)
		{
			eye = eye + step * lookDir;
		}

		/// Rotates the camera counterclockwise around its UP vector. 
		inline void yaw(double angleDegrees)
		{
			glm::mat4 R = glm::rotate(glm::dmat4(1.0), glm::radians(angleDegrees), upDir);
			glm::dvec4 v = R * glm::dvec4(lookDir.x, lookDir.y, lookDir.z, 0.0);
			lookDir = glm::normalize(glm::dvec3(v.x, v.y, v.z));
		}

		/// Rotates the camera counterclockwise around its LEFT vector (a positive angle means "nose down"). 
		inline void pitch(double angleDegrees)
		{
			glm::dvec3 left = glm::cross(upDir, lookDir);
			glm::dmat4 R = glm::rotate(glm::dmat4(1.0), glm::radians(angleDegrees), left);
			glm::dvec4 v = R * glm::dvec4(lookDir.x, lookDir.y, lookDir.z, 0.0);
			lookDir = glm::normalize(glm::dvec3(v.x, v.y, v.z));
			v = R * glm::dvec4(upDir.x, upDir.y, upDir.z, 0.0);
			upDir = glm::normalize(glm::dvec3(v.x, v.y, v.z));
		}

		/// Rotates the camera counterclockwise around its Forward vector. 
		inline void roll(double angleDegrees)
		{
			glm::dmat4 R = glm::rotate(glm::dmat4(1.0), glm::radians(angleDegrees), lookDir);
			glm::dvec4 v = R * glm::dvec4(upDir.x, upDir.y, upDir.z, 0.0);
			upDir = glm::normalize(glm::dvec3(v.x, v.y, v.z));
		}

		inline glm::dvec3 getPosition() {
			return eye;
		}

		inline void setPosition(const glm::dvec3& position) {
			eye = position;
		}

		inline void setUpDirection(const glm::dvec3& direction) {
			upDir = glm::normalize(direction);
		}

		inline void setForwardDirection(const glm::dvec3& direction) {
			lookDir = glm::normalize(direction);
		}

		inline void setViewTarget(const glm::dvec3& position) {
			lookDir = glm::normalize(position - eye);
			glm::dvec3 right = glm::cross(lookDir, upDir);
			upDir = glm::cross(right, lookDir);
		}
	};

}