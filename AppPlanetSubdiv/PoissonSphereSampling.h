#pragma once


#include "tool.h"
#include <vector>


class PoissonSphereSampling
{
public:
	PoissonSphereSampling() {}
	~PoissonSphereSampling() {}

	void sample(double sphere_radius, double poisson_radius);
	inline std::vector<math::dvec3>& getSamples() { return m_samples; }

private:

	std::vector<math::dvec3> m_samples;
	double m_sphere_radius;
	double m_poisson_radius;
};