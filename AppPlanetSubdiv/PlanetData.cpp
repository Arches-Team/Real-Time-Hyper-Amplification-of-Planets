#include "PlanetData.h"

#include <fstream>
#include <iostream>
#include <vector>



bool PlanetData::loadFromTectonicFile(const std::string & filename)
{
	// read data from disk:
	std::ifstream file(filename.c_str(), std::ifstream::in | std::ifstream::binary);
	if (!file.good())
	{
		std::cout << "ERROR - PlanetData:: failed opening " << filename << std::endl;
		return false;
	}

	file.read((char*)&radiusKm, sizeof(double));
	file.read((char*)&seaLevelKm, sizeof(double));
	int num_plates;
	file.read((char*)&num_plates, sizeof(int));
	file.read((char*)&m_num_vertices, sizeof(int));
	file.read((char*)&m_num_triangles, sizeof(int));

	m_vertices = new PersistentTectonicVertex[m_num_vertices];
	
	for (int i = 0; i < m_num_vertices; ++i)
	{
		file.read((char*)&m_vertices[i], sizeof(PersistentTectonicVertex));	
		m_vertices[i].elevation += 0.1;//add x meters
	}

	m_triangles = new PersistentTectonicTriangle[m_num_triangles];
	for (int i = 0; i < m_num_triangles; ++i)
		file.read((char*)(m_triangles + i), sizeof(PersistentTectonicTriangle));

	file.close();

	// create BVH:
	std::vector<tool::Triangle> tris;
	tris.reserve(m_num_triangles);
	for (int i = 0; i < m_num_triangles; ++i)
	{
		const PersistentTectonicVertex & v0 = m_vertices[m_triangles[i].vertex[0]];
		math::dvec3 p0(v0.surface_coordinates[0], v0.surface_coordinates[1], v0.surface_coordinates[2]);

		const PersistentTectonicVertex & v1 = m_vertices[m_triangles[i].vertex[1]];
		math::dvec3 p1(v1.surface_coordinates[0], v1.surface_coordinates[1], v1.surface_coordinates[2]);

		const PersistentTectonicVertex & v2 = m_vertices[m_triangles[i].vertex[2]];
		math::dvec3 p2(v2.surface_coordinates[0], v2.surface_coordinates[1], v2.surface_coordinates[2]);

		tool::Triangle T(p0, p1, p2);
		T.ref = i;
		tris.push_back(T);
	}

	m_bvh = new tool::BVH(tris);

	//
	return true;
}

bool PlanetData::loadFromMaps(const std::string & path)
{
	m_use_maps = true;

	// --- Parse Header File ---
	std::string headerfile = path + std::string("header.txt");
	FILE * header = std::fopen(headerfile.c_str(), "r");
	if (header == NULL)
	{
		std::cout << "Error : unable to open header file." <<std::endl;
		return false;
	}

	const int CHARNUM = 511;
	char line[CHARNUM + 1];
	char val[128];
	do
	{
		std::fgets(line, CHARNUM, header);
	} while (line[0] == '#' || line[0] == '\n');

	std::sscanf(line, "%s", val);
	std::string continents_mapname(val);
	m_map_continent.load(path + continents_mapname);
	m_map_continent.setProjection(ProjectedMap::Projection::WAGNER_VI);

	do
	{
		std::fgets(line, CHARNUM, header);
	} while (line[0] == '#' || line[0] == '\n');

	std::sscanf(line, "%s", val);
	std::string elevations_mapname(val);
	m_map_elevation.load(path + elevations_mapname);
	m_map_elevation.setProjection(ProjectedMap::Projection::EQUIRECTANGULAR);

	do
	{
		std::fgets(line, CHARNUM, header);
	} while (line[0] == '#' || line[0] == '\n');

	std::sscanf(line, "%s", val);
	std::string humidity_mapname(val);
	m_map_humidity.load(path + humidity_mapname);
	m_map_humidity.setProjection(ProjectedMap::Projection::WAGNER_VI);

	std::fclose(header);

	return true;
}

PlanetData::Data PlanetData::getInterpolatedModelData(const math::dvec3 & surface_coordinates) const
{
	Data data;
	data.strain_direction = math::dvec3(1.0, 0.0, 0.0);
	data.strain_level = 0.0;
	data.base_elevation = 0.0;
	data.elevation = 0.0;
	data.age = 0.0;

	if (m_use_maps)
	{
		QRgb pix = m_map_continent.sample(surface_coordinates);
		int r = qRed(pix);
		int g = qGreen(pix);
		int b = qBlue(pix);
		bool continental = (r == 0 && g == 0 && b == 0);

		pix = m_map_elevation.sample(surface_coordinates, 3.141592653589793 / 17.5);
		double normed_altitude = (double)(qRed(pix)) / 255.0;

		if (continental)
			data.elevation = seaLevelKm + 0.01 + normed_altitude * maxAltitude;

		return data;
	}

	// Else, using a Tectonic File:

	math::dvec3 N = math::normalize(surface_coordinates);
	
	// intersect planet model using BVH:
	tool::Ray R(math::dvec3(0.0, 0.0, 0.0), N);
	std::vector<tool::Triangle::Intersection> hits;
	if (!m_bvh->intersectRay(R, hits)) // Should not Happen FIXME
	{
		// jitter position and retry:
		tool::MonoVectorFrame tangentFrame(N);
		math::dvec3 jittered = surface_coordinates + tangentFrame.t * 0.5;
		return getInterpolatedModelData(jittered);		
	}

	// Interpolate and return:
	double u = 1.0 - hits[0].u - hits[0].v;
	double v = hits[0].u;
	double w = hits[0].v;

	const PersistentTectonicTriangle & ptt = m_triangles[hits[0].ref];

	const PersistentTectonicVertex & pv0 = m_vertices[ptt.vertex[0]];
	const PersistentTectonicVertex & pv1 = m_vertices[ptt.vertex[1]];
	const PersistentTectonicVertex & pv2 = m_vertices[ptt.vertex[2]];

	math::dvec3 fd0(pv0.strain_direction[0], pv0.strain_direction[1], pv0.strain_direction[2]);
	math::dvec3 fd1(pv1.strain_direction[0], pv1.strain_direction[1], pv1.strain_direction[2]);
	math::dvec3 fd2(pv2.strain_direction[0], pv2.strain_direction[1], pv2.strain_direction[2]);

	data.strain_direction = math::normalize(u * fd0 + v * fd1 + w * fd2);
	data.strain_level = u * pv0.strain_level + v * pv1.strain_level + w * pv2.strain_level;
	data.base_elevation = u * pv0.base_elevation + v * pv1.base_elevation + w * pv2.base_elevation;//TODO : apply transfer functions for plateaux
	data.elevation = u * pv0.elevation + v * pv1.elevation + w * pv2.elevation;
	data.age = u * pv0.age + v * pv1.age + w * pv2.age;

	return data;
}

float PlanetData::getPlateauxDistribution(const math::dvec3 & position) const
{
	if (m_use_maps)
		return 0.0;

	math::dvec3 p = math::normalize(position);
	double noise1 = tool::fractalRidged3D(3, 0.5, 2.02, 6.0, p);
	noise1 *= tool::fractalSimplex3D(4, 0.5, 2.02, 9.0, p + math::dvec3(2.26));
	noise1 = (math::clamp(noise1, 0.15, 0.85) - 0.15) / 0.7; // stretch to cover range [0, 1]

	return (float)noise1;	
}

float PlanetData::getDesertDistribution(const math::dvec3 & position) const
{
	float desert;

	if (m_use_maps)
	{
		QRgb pix = m_map_humidity.sample(position, 3.141592653589793);// / 17.5);
		desert = (float)(qRed(pix)) / 255.0f;

		return desert;
	}

	const math::dvec3 p = math::normalize(position) * radiusKm;
	
	const float noise1 = (float)tool::fractalSimplex3D(9, 0.52, 2.02, 0.0002, p + math::dvec3(3005.3));
	const float noise2 = (float)tool::fractalRidged3D(9, 0.53, 2.1, 0.0016, p + math::dvec3(1500.0));
	desert = math::smoothstep(0.54f, 0.7f, noise1);//
	desert *= math::mix(noise2*noise2, 1.0f, desert*desert);

	return desert;
}

float PlanetData::getHillsDistribution(const math::dvec3 & position) const
{
	if (m_use_maps)
		return 0.0;
	
	const math::dvec3 p = math::normalize(position) * radiusKm;

	const float noise1 = (float)tool::fractalRidged3D(3, 0.53, 2.02, 0.0035, p + math::dvec3(-1770.0));
	const float noise2 = (float)tool::fractalSimplex3D(2, 0.52, 2.02, 0.0003, p + math::dvec3(2005.0));
	float hills = math::smoothstep(0.5f, 0.8f, noise1) * math::smoothstep(0.5f, 0.55f, noise2);
	return hills;
}

void PlanetData::release()
{
	delete[] m_triangles;
	delete[] m_vertices;
	delete m_bvh;
}

QRgb ProjectedMap::sample(const math::dvec3 & spherepos, double LongitudeManualOffset) const
{
	if (!m_img_loaded)
		return 0;

	const double Pi = 3.141592653589793;
	const math::dvec3 npos = math::normalize(spherepos);

	double latitude = std::asin(npos.z);
	double longitude = 0.0;
	double coslat = std::cos(latitude);
	if (coslat != 0.0)
		longitude = std::acos(npos.x / coslat);
	if (npos.y < 0.0)
		longitude = -longitude;

	int X, Y;
	if (m_projection == Projection::WAGNER_VI)
	{
		// Wagner VI projection				
		double wagnercoeff = std::sqrt(1.0 - 3.0 * std::pow(latitude / Pi, 2.0));
		Y = (int)((1.0 - (0.5 + latitude / Pi)) * (double)m_img.height() - 1.0);
		double x = 0.5*(1.0 + longitude * wagnercoeff / Pi);//fit [0, 1]
		X = (int)(x * (double)(m_img.width() - 1));
	}
	else if (m_projection == Projection::EQUIRECTANGULAR)
	{
		longitude += LongitudeManualOffset;//calibrate because of parallels mismatch 
		if (longitude > Pi)
			longitude -= 2.0 * Pi;
		int Xr, Yr;
		double xr = 0.5 * (1.0 + longitude / Pi);
		double yr = 1.0 - (0.5 + latitude / Pi);
		X = (int)(xr * (double)(m_img.width() - 1));
		Y = (int)(yr * (double)(m_img.height() - 1));
	}

	return m_img.pixel(X, Y);
}

bool ProjectedMap::load(const std::string & filename)
{
	if (!m_img.load(QString(filename.c_str())))
		return false;

	m_img_loaded = true;
	return true;
}
