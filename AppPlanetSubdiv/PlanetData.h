#pragma once

#include "tool.h"

#include <qimage.h>

#include <string>




class ProjectedMap
{
public:
	
	enum class Projection : int
	{
		NONE,
		WAGNER_VI,
		EQUIRECTANGULAR
	};

	QRgb sample(const math::dvec3 & spherepos, double LongitudeManualOffset = 0.0) const;

	bool load(const std::string & filename);

	inline void setProjection(Projection p) { m_projection = p; }
	
private:

	QImage m_img;
	Projection m_projection = Projection::NONE;
	bool m_img_loaded = false;
};


// A tectonic triangle, persistable to disk.
struct PersistentTectonicTriangle
{
	int vertex[3];
};


// A tectonic vertex, persistable to disk, and used for rendering
struct PersistentTectonicVertex
{
	/// This vertex 3D coordinates
	double surface_coordinates[3];
	/// Local direction of strain (folds for continents, transform faults for oceanic)
	double strain_direction[3];
	/// Local strain level, a value in [0, 1]
	double strain_level;
	/// Local base elevation
	double base_elevation;
	/// Local maximum elevation
	double elevation;
	/// Age of relief (for continental mountains) or age of sea floor (when undersea), in Ma
	double age;
	/// Distinguishing between collision (1.0) and subduction (0.0)
	double orogeny_type;
};





struct PlanetData
{
	double radiusKm = 6370.0;
	double seaLevelKm = 10.0;
	double atmosphereDepthKm = 90.0;
	double maxAltitude = 10.0;
	unsigned int seed = 13337;

	bool loadFromTectonicFile(const std::string & filename);
	bool loadFromMaps(const std::string & directory);

	~PlanetData() { release(); }

	/**
	* @param surface_coordinates The point to sample
	*/
	struct Data
	{
		math::dvec3 strain_direction;
		double strain_level;
		double base_elevation, elevation;
		double age;
	};

	PlanetData::Data getInterpolatedModelData(const math::dvec3 & surface_coordinates) const;
	float getPlateauxDistribution(const math::dvec3 & position) const;
	float getDesertDistribution(const math::dvec3 & position) const;
	float getHillsDistribution(const math::dvec3 & position) const;

private:
	
	void release();
	

private:

	std::string m_filename;
	PersistentTectonicVertex * m_vertices = nullptr;
	PersistentTectonicTriangle * m_triangles = nullptr;
	tool::BVH * m_bvh = nullptr;
	int m_num_vertices = 0, m_num_triangles = 0;

	ProjectedMap m_map_continent, m_map_elevation, m_map_humidity, m_map_age;


	bool m_use_maps = false;
};