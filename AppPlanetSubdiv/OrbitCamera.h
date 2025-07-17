#pragma once

#include "tool.h"
#include <string>

namespace tool
{


	struct OrbitCameraDouble
	{
		glm::dmat4 projection;
		glm::dvec3 eye;
		glm::dvec3 center;
		glm::dvec3 up;
		double longitude, latitude, radius;
		double minCameraRadius;

		inline OrbitCameraDouble()
			: eye(1.0, 0.0, 0.0), center(0.0, 0.0, 0.0), up(0.0, 0.0, 1.0)
			, longitude(0.0), latitude(0.0), radius(1.0), minCameraRadius(1e-6)
		{
		}
		inline OrbitCameraDouble(double orbitRadius, const glm::dvec3 & orbitCenter = glm::dvec3(0.0, 0.0, 0.0))
			: eye(orbitRadius, 0.0, 0.0)
			, center(0.0, 0.0, 0.0)
			, up(0.0, 0.0, 1.0)
			, longitude(0.0)
			, latitude(0.0)
			, radius(orbitRadius)
			, minCameraRadius(1e-6)
		{
			setCenter(orbitCenter);
		}
		inline ~OrbitCameraDouble()
		{}

		inline OrbitCameraDouble& operator=(OrbitCameraDouble const& c) {
			eye = c.eye;
			center = c.center;
			up = c.up;
			longitude = c.longitude;
			latitude = c.latitude;
			radius = c.radius;
			minCameraRadius = c.minCameraRadius;
			
			return *this;
		}

		/** @return the OpenGL compatible view matrix associated with the camera's current state. */
		inline glm::dmat4 getViewMatrix()
		{
			return glm::lookAt(eye, center, up);
		}

		inline glm::dmat4 perspective(double verticalFOVDegrees, double viewRatio, double nearplane, double farplane)
		{
			projection = glm::perspective(glm::radians(verticalFOVDegrees), viewRatio, nearplane, farplane);
			return projection;
		}

		/* Value below 1 means zooming in, above 1 means zooming out
		*/
		inline void zoom(double zoomcoeff)
		{
			double oldRadius = radius;
			radius *= zoomcoeff;
			if (radius < minCameraRadius)
				radius = minCameraRadius;
			eye = center + (radius / oldRadius) * (eye - center);
		}

		inline void setOrbitRadius(double orbitRadius)
		{
			radius = orbitRadius;
			if (radius < minCameraRadius)
				radius = minCameraRadius;
			eye = center + radius * (eye - center);
		}

		inline void orbit(double incr_longitude, double incr_latitude)
		{
			/*constexpr double Deg2Rad = 3.1415926535897932384626433832795 / 180.0;

			longitude += incr_longitude;
			longitude += (longitude < -360.0) ? 360.0 : ((longitude > 360.0) ? -360.0 : 0.0);
			latitude += incr_latitude;
			latitude += (latitude < -360.0) ? 360.0 : ((latitude > 360.0) ? -360.0 : 0.0);

			glm::dmat4 rot = glm::rotate(glm::dmat4(1.0), longitude * Deg2Rad, glm::dvec3(0.0, 0.0, 1.0));
			glm::dvec3 axis = rot * glm::dvec4(0.0, 1.0, 0.0, 0.0);
			glm::dmat4 rot2 = glm::rotate(glm::dmat4(1.0), -latitude * Deg2Rad, axis);
			glm::dmat4 R = rot2 * rot;
			up = R * glm::dvec4(0.0, 0.0, 1.0, 0.0);
			eye = R * glm::dvec4(radius, 0.0, 0.0, 1.0);
			eye += center;
			*/
			longitude += incr_longitude;
			longitude += (longitude < -360.0) ? 360.0 : ((longitude > 360.0) ? -360.0 : 0.0);
			latitude += incr_latitude;
			latitude += (latitude < -360.0) ? 360.0 : ((latitude > 360.0) ? -360.0 : 0.0);

			glm::dmat4 rot = glm::rotate(glm::dmat4(1.0), glm::radians(longitude), glm::dvec3(0.0, 0.0, 1.0));
			glm::dvec4 axis = rot * glm::dvec4(0.0, 1.0, 0.0, 0.0);
			glm::dmat4 rot2 = glm::rotate(glm::dmat4(1.0), glm::radians(-latitude), glm::dvec3(axis.x, axis.y, axis.z));
			rot = rot2 * rot;

			glm::dvec4 up4 = rot * glm::dvec4(0.0, 0.0, 1.0, 0.0);
			up = glm::dvec3(up4.x, up4.y, up4.z);
			glm::dvec4 e4 = rot * glm::dvec4(radius, 0.0, 0.0, 1.0);
			eye = glm::dvec3(e4.x, e4.y, e4.z);
			eye += center;
		}

		inline void orbitTo(double target_longitude, double target_latitude)
		{
			/*constexpr double Deg2Rad = 3.1415926535897932384626433832795 / 180.0;

			longitude = target_longitude;
			longitude += (longitude < -360.0) ? 360.0 : ((longitude > 360.0) ? -360.0 : 0.0);
			latitude = target_latitude;
			latitude += (latitude < -360.0) ? 360.0 : ((latitude > 360.0) ? -360.0 : 0.0);

			glm::dmat4 rot = glm::rotate(glm::dmat4(1.0), longitude * Deg2Rad, glm::dvec3(0.0, 0.0, 1.0));
			glm::dvec3 axis = rot * glm::dvec4(0.0, 1.0, 0.0, 0.0);
			glm::dmat4 rot2 = glm::rotate(glm::dmat4(1.0), -latitude * Deg2Rad, axis);
			glm::dmat4 R = rot2 * rot;
			up = R * glm::dvec4(0.0, 0.0, 1.0, 0.0);
			eye = R * glm::dvec4(radius, 0.0, 0.0, 1.0);
			eye += center;
			*/
			longitude = target_longitude;
			longitude += (longitude < -360.0) ? 360.0 : ((longitude > 360.0) ? -360.0 : 0.0);
			latitude = target_latitude;
			latitude += (latitude < -360.0) ? 360.0 : ((latitude > 360.0) ? -360.0 : 0.0);

			glm::dmat4 rot = glm::rotate(glm::dmat4(1.0), glm::radians(longitude), glm::dvec3(0.0, 0.0, 1.0));
			glm::dvec4 axis = rot * glm::dvec4(0.0, 1.0, 0.0, 0.0);
			glm::dmat4 rot2 = glm::rotate(glm::dmat4(1.0), glm::radians(-latitude), glm::dvec3(axis.x, axis.y, axis.z));
			rot = rot2 * rot;

			glm::dvec4 up4 = rot * glm::dvec4(0.0, 0.0, 1.0, 0.0);
			up = glm::dvec3(up4.x, up4.y, up4.z);
			glm::dvec4 e4 = rot * glm::dvec4(radius, 0.0, 0.0, 1.0);
			eye = glm::dvec3(e4.x, e4.y, e4.z);
			eye += center;
		}

		inline glm::dvec3 getPosition() {
			return eye;
		}

		inline void setCenter(const glm::dvec3 & orbitCenter) {
			eye = eye + (orbitCenter - center);
			center = orbitCenter;
		}

		inline void setMinCameraRadius(double mr) {
			if (mr > 0.0)
				minCameraRadius = mr;
		}
	};

}//end:namespace
