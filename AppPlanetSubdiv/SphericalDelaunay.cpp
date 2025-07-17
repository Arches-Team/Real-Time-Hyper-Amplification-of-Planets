#include "SphericalDelaunay.h"

#include <cmath>
#include <unordered_map>
#include <fstream>


bool SphericalTriangle::containsPoint(const SphericalVertex * vertices, const math::dvec3 & sphere_center, const math::dvec3 & v) const
{
	
	// look-up the three vertices coordinates:
	const math::dvec3 & v0 = vertices[vertex[0]].coordinates;
	const math::dvec3 & v1 = vertices[vertex[1]].coordinates;
	const math::dvec3 & v2 = vertices[vertex[2]].coordinates;

	// compute the three normals:
	math::dvec3 O0 = math::normalize(v0 -sphere_center);
	math::dvec3 O1 = math::normalize(v1 -sphere_center);
	math::dvec3 O2 = math::normalize(v2 -sphere_center);

	math::dvec3 n0 = math::cross(O1, O2); // cross product
	math::dvec3 n1 = math::cross(O2, O0); // cross product
	math::dvec3 n2 = math::cross(O0, O1); // cross product

	// Test:
	bool hemisphereTest = math::dot(math::normalize(v - sphere_center), O0) > 0.0;
	bool test0 = math::dot(math::normalize(v - v1), n0) >= EPSILON_TEST;
	bool test1 = math::dot(math::normalize(v - v2), n1) >= EPSILON_TEST;
	bool test2 = math::dot(math::normalize(v - v0), n2) >= EPSILON_TEST;
	return hemisphereTest && test0 && test1 && test2;
	
	/*const math::dvec3 & v0 = vertices[vertex[0]].coordinates;
	const math::dvec3 & v1 = vertices[vertex[1]].coordinates;
	const math::dvec3 & v2 = vertices[vertex[2]].coordinates;
	tool::Triangle T(v0, v1, v2);
	tool::Ray R(sphere_center, math::normalize(v - sphere_center));
	double t, u, w;
	return T.intersect(R, DBL_MAX, t, u, w);*/
}

void SphericalVertex::getIncidentTriangles(int vertex_index, const SphericalTriangle * triangles, std::set<int> & incident) const
{
	incident.insert(triangle);

	constexpr int MAXL = 32;
	int list[MAXL];
	int current = 0;
	list[current] = triangle;
	int size = 1;

	while (true)
	{
		bool added = false;
		const SphericalTriangle * T = triangles + list[current];
		
		int i = 0;
		for (; i < 3; ++i)
			if (T->vertex[i] == vertex_index)
				break;
		assert(i < 3);

		int t1 = T->neighbor[(i + 1) % 3];
		int t2 = T->neighbor[(i + 2) % 3];
		if (t1 != -1 && !TRIANGLE_DELETED(triangles[t1]) && incident.find(t1) == incident.end())
		{
			incident.insert(t1);
			added = true;
			assert(size < MAXL);
			list[size] = t1;			
			size++;			
		}
		if (t2 != -1 && !TRIANGLE_DELETED(triangles[t2]) && incident.find(t2) == incident.end())
		{
			incident.insert(t2);
			added = true;
			assert(size < MAXL);
			list[size] = t2;			
			size++;			
		}

		if (!added && current == size -1)
			return;

		current++;
	}
}

SphericalDelaunay::SphericalDelaunay(double sphere_radius, unsigned int reserve)
	: m_vertex_index(0), m_triangle_index(0)
	, m_num_vertices(0), m_num_triangles(0)
	, m_sphere_center(0.0, 0.0, 0.0), m_sphere_radius(sphere_radius)
{
	if (reserve == 0)
		reserve = 8;

	m_vertices = new SphericalVertex[reserve];
	m_triangles = new SphericalTriangle[4 * reserve];
	m_triangle_capacity = 4 * reserve;
	m_vertex_capacity = reserve;

	m_vertices_freeslots.reserve(64);
	m_triangles_freeslots.reserve(64);

	makeBaseOctahedron();
}

SphericalDelaunay::SphericalDelaunay(double sphere_radius, double poisson_radius)
	: m_vertex_index(0), m_triangle_index(0)
	, m_num_vertices(0), m_num_triangles(0)
	, m_sphere_center(0.0, 0.0, 0.0), m_sphere_radius(sphere_radius)
{
	PoissonSphereSampling sampler;
	sampler.sample(m_sphere_radius, poisson_radius);
	std::vector<math::dvec3> samples = sampler.getSamples();
	std::vector<math::dvec3> samples_without_octahedron(samples.begin() + 6, samples.end());

	int reserve = samples.size() +2;
	m_vertices = new SphericalVertex[reserve];
	m_triangles = new SphericalTriangle[4 * reserve];
	m_triangle_capacity = 4 * reserve;
	m_vertex_capacity = reserve;

	m_vertices_freeslots.reserve(64);
	m_triangles_freeslots.reserve(64);
	
	// outline:
	// 1 - do a naive triangulation (just find the host triangle and split)
	// 2 - make a Lawson pass (edge flips)
	makeBaseOctahedron();
	const int naive_count = std::min((int)samples_without_octahedron.size() - 1, 10000);
	std::vector<math::dvec3> samples_naive(samples_without_octahedron.begin(), samples_without_octahedron.begin() + naive_count);
	createNaiveTriangulation(samples_naive);
	applyLawson();

	std::cout << "Incremental delaunay insertion ... " << std::endl;
	const int count = samples_without_octahedron.size() - naive_count;
	auto it = samples_without_octahedron.begin() + naive_count;
	for (int i = 0; i < count; ++i)
	{
		insertVertex(*it);
		//if (i % 1024 == 0)
			//std::cout << i << "/" << count << " ";
		it++;
	}
	std::cout << "\n" << std::endl;
}

/*SphericalDelaunay::SphericalDelaunay(const std::vector<math::dvec3> & vertices, double sphere_radius)
	: SphericalDelaunay(sphere_radius, vertices.size() + 1)
{
	// outline:
	// 1 - do a naive triangulation (just find the host triangle and split)
	// 2 - make a Lawson pass (edge flips)
	createNaiveTriangulation(vertices);
	applyLawson();
}*/

SphericalDelaunay::SphericalDelaunay(const SphericalDelaunay & sdt)
{	
	m_sphere_center = sdt.m_sphere_center;
	m_sphere_radius = sdt.m_sphere_radius;	

	if (sdt.m_num_triangles > 0)
	{
		m_num_triangles = sdt.m_num_triangles;
		m_triangle_id_count = sdt.m_triangle_id_count;
		m_triangle_index = sdt.m_triangle_index;
		m_triangle_capacity = sdt.m_triangle_capacity;
		m_triangles = new SphericalTriangle[m_triangle_capacity];
		for (int i = 0; i < m_triangle_index; ++i)
			m_triangles[i] = sdt.m_triangles[i];			
		m_triangles_freeslots = std::vector<int>(sdt.m_triangles_freeslots.begin(), sdt.m_triangles_freeslots.end());
	}
	if (sdt.m_num_vertices > 0)
	{
		m_num_vertices = sdt.m_num_vertices;
		m_vertex_id_count = sdt.m_vertex_id_count;
		m_vertex_index = sdt.m_vertex_index;
		m_vertex_capacity = sdt.m_vertex_capacity;
		m_vertices = new SphericalVertex[m_vertex_capacity];
		for (int i = 0; i < m_vertex_index; ++i)
			m_vertices[i] = sdt.m_vertices[i];
		m_vertices_freeslots = std::vector<int>(sdt.m_vertices_freeslots.begin(), sdt.m_vertices_freeslots.end());
	}
}

SphericalDelaunay & SphericalDelaunay::operator=(const SphericalDelaunay & sdt)
{
	m_sphere_center = sdt.m_sphere_center;
	m_sphere_radius = sdt.m_sphere_radius;

	if (sdt.m_num_triangles > 0)
	{
		m_num_triangles = sdt.m_num_triangles;
		m_triangle_id_count = sdt.m_triangle_id_count;
		m_triangle_index = sdt.m_triangle_index;
		m_triangle_capacity = sdt.m_triangle_capacity;
		m_triangles = new SphericalTriangle[m_triangle_capacity];
		for (int i = 0; i < m_triangle_index; ++i)
			m_triangles[i] = sdt.m_triangles[i];
		m_triangles_freeslots = std::vector<int>(sdt.m_triangles_freeslots.begin(), sdt.m_triangles_freeslots.end());
	}
	if (sdt.m_num_vertices > 0)
	{
		m_num_vertices = sdt.m_num_vertices;
		m_vertex_id_count = sdt.m_vertex_id_count;
		m_vertex_index = sdt.m_vertex_index;
		m_vertex_capacity = sdt.m_vertex_capacity;
		m_vertices = new SphericalVertex[m_vertex_capacity];
		for (int i = 0; i < m_vertex_index; ++i)
			m_vertices[i] = sdt.m_vertices[i];
		m_vertices_freeslots = std::vector<int>(sdt.m_vertices_freeslots.begin(), sdt.m_vertices_freeslots.end());
	}

	return *this;
}

void SphericalDelaunay::cleanup()
{
	delete[] m_vertices;
	m_vertices = nullptr;
	delete[] m_triangles;
	m_triangles = nullptr;

	m_num_vertices = 0;
	m_vertex_index = 0;
	m_vertex_capacity = 0;
	m_vertex_id_count = 0;
	m_num_triangles = 0;
	m_triangle_index = 0;
	m_triangle_capacity = 0;
	m_triangle_id_count = 0;
		
	m_vertices_freeslots.clear();
	m_triangles_freeslots.clear();
}



void SphericalDelaunay::changeSphereRadius(double new_radius)
{
	for (int i = 0; i < m_vertex_index; ++i)
	{
		if (VERTEX_DELETED(m_vertices[i]))
			continue;
		math::dvec3 v = m_vertices[i].coordinates - m_sphere_center;
		v = math::normalize(v);
		v *= new_radius;
		m_vertices[i].coordinates = m_sphere_center  +  v;
	}
	m_sphere_radius = new_radius;
}

int SphericalDelaunay::insertVertexAtTriangle(const math::dvec3 & vertex, int triangle_index)
{
	int target_triangle_index = triangle_index;

	int new_vertex_index = findAvailableVertexSlot();
	m_vertices[new_vertex_index].coordinates = vertex;
	m_vertices[new_vertex_index].flags = 0;
	m_vertices[new_vertex_index].id = m_vertex_id_count;	
	//for (int i = 0; i < MAX_INCIDENT_TRIANGLES; ++i)
		//m_vertices[new_vertex_index].triangle[i] = -1;
	m_vertex_id_count++;
	m_num_vertices++;	
	SphericalTriangle target_triangle = m_triangles[target_triangle_index];
	
	// Create 3 new sub triangles:
	SphericalTriangle t0, t1, t2;
	int index_t0 = target_triangle_index;
	int index_t1 = findAvailableTriangleSlot();
	int index_t2 = findAvailableTriangleSlot();

	t0.vertex[0] = new_vertex_index;
	t0.vertex[1] = target_triangle.vertex[0];
	t0.vertex[2] = target_triangle.vertex[1];

	t0.neighbor[0] = target_triangle.neighbor[2];
	t0.neighbor[1] = index_t1;
	t0.neighbor[2] = index_t2;

	t1.vertex[0] = new_vertex_index;
	t1.vertex[1] = target_triangle.vertex[1];
	t1.vertex[2] = target_triangle.vertex[2];

	t1.neighbor[0] = target_triangle.neighbor[0];
	if (t1.neighbor[0] != -1)
	{   // update neighborhood
		SphericalTriangle & t1_v0 = m_triangles[t1.neighbor[0]];
		for (int k = 0; k <3; ++k)
			if (t1_v0.neighbor[k] == target_triangle_index) {
				t1_v0.neighbor[k] = index_t1;
				break;
			}
	}
	t1.neighbor[1] = index_t2;
	t1.neighbor[2] = index_t0;

	t2.vertex[0] = new_vertex_index;
	t2.vertex[1] = target_triangle.vertex[2];
	t2.vertex[2] = target_triangle.vertex[0];

	t2.neighbor[0] = target_triangle.neighbor[1];
	if (t2.neighbor[0] != -1)
	{   // update neighborhood
		SphericalTriangle & t2_v0 = m_triangles[t2.neighbor[0]];
		for (int k = 0; k <3; ++k)
			if (t2_v0.neighbor[k] == target_triangle_index) {
				t2_v0.neighbor[k] = index_t2;
				break;
			}
	}
	t2.neighbor[1] = index_t0;
	t2.neighbor[2] = index_t1;

	m_triangles[index_t0] = t0;
	m_triangles[index_t1] = t1;
	m_triangles[index_t2] = t2;	

	m_vertices[new_vertex_index].triangle = index_t0;
	m_vertices[target_triangle.vertex[2]].triangle = index_t2;
	/*m_vertices[new_vertex_index].addIncidentTriangle(index_t0);
	m_vertices[new_vertex_index].addIncidentTriangle(index_t1);
	m_vertices[new_vertex_index].addIncidentTriangle(index_t2);
	m_vertices[target_triangle.vertex[0]].addIncidentTriangle(index_t2);
	m_vertices[target_triangle.vertex[1]].addIncidentTriangle(index_t1);
	m_vertices[target_triangle.vertex[2]].replaceIndicentTriangle(index_t0, index_t2);
	m_vertices[target_triangle.vertex[2]].addIncidentTriangle(index_t1);*/

	m_num_triangles += 2;

	// Build a list of edges to test and possibly flip:
	std::list<std::pair<int, int>> list;
	list.push_back(std::make_pair(index_t0, m_triangles[index_t0].neighbor[0]));
	list.push_back(std::make_pair(index_t1, m_triangles[index_t1].neighbor[0]));
	list.push_back(std::make_pair(index_t2, m_triangles[index_t2].neighbor[0]));

	int flipcount = 0;
	// Process the list:
	while (!list.empty())
	{
		std::pair<int, int> edge = list.front();
		list.pop_front();

		if (edge.second == -1 || edge.first == -1)
			continue;// we don't flip edges that are on the border of the triangulation

		int j;// index of the non-shared vertex in triangle edge.first
		for (j = 0; j < 3; ++j)
		{
			int s = m_triangles[edge.first].vertex[j];
			bool found = true;
			for (int i = 0; i < 3; ++i)
				if (m_triangles[edge.second].vertex[i] == s) { found = false; break; }
			if (found)
				break;
		}
		SphericalEdge e;
		e.vertex[0] = m_triangles[edge.first].vertex[(j + 1) % 3];
		e.vertex[1] = m_triangles[edge.first].vertex[(j + 2) % 3];
		e.triangle[0] = edge.first;
		e.triangle[1] = edge.second;

		if (!checkEdgeDelaunay(e)) //-> flip
		{
			int s, s0, s1, s2;//unused
			flipEdge(e, s, s0, s1, s2);
			flipcount++;
			//std::cout << " " << flipcount << "x ";

			// ajout à la file des 2 arêtes adjacentes ne contenant pas le nouveau point:
			int k;
			for (k = 0; k <3; ++k)
				if (m_triangles[edge.first].vertex[k] == new_vertex_index)
					break;
			list.push_back(std::make_pair(edge.first, m_triangles[edge.first].neighbor[k]));

			for (k = 0; k <3; ++k)
				if (m_triangles[edge.second].vertex[k] == new_vertex_index)
					break;
			list.push_back(std::make_pair(edge.second, m_triangles[edge.second].neighbor[k]));
		}
	}	
	
	return new_vertex_index;
}

int SphericalDelaunay::naiveInsertVertex(const math::dvec3 & vertex)
{
	int index = smartFindTriangleThatContains(vertex);
	//if (index == -1)
		//return -1;
	return naiveInsertVertexAtTriangle(vertex, index);
}

int SphericalDelaunay::naiveInsertVertexAtTriangle(const math::dvec3 & vertex, int triangle_index)
{
	int v_index = findAvailableVertexSlot();
	m_vertices[v_index].coordinates = vertex;
	m_vertices[v_index].flags = 0;
	m_vertices[v_index].id = m_vertex_id_count;
	//for (int i = 0; i < MAX_INCIDENT_TRIANGLES; ++i)
		//m_vertices[v_index].triangle[i] = -1;
	m_vertex_id_count++;
	m_num_vertices++;

	int i = triangle_index;
	SphericalTriangle target = m_triangles[i];

	// create 3 new triangles:
	SphericalTriangle t0, t1, t2;
	int t0_index = i;
	int t1_index = findAvailableTriangleSlot();
	int t2_index = findAvailableTriangleSlot();

	t0.vertex[0] = target.vertex[0];
	t0.vertex[1] = target.vertex[1];
	t0.vertex[2] = v_index;
	t0.neighbor[0] = t1_index;
	t0.neighbor[1] = t2_index;
	t0.neighbor[2] = target.neighbor[2];
	t0.flags = 0;
	m_triangles[t0_index] = t0;

	t1.vertex[0] = target.vertex[1];
	t1.vertex[1] = target.vertex[2];
	t1.vertex[2] = v_index;
	t1.neighbor[0] = t2_index;
	t1.neighbor[1] = t0_index;
	t1.neighbor[2] = target.neighbor[0];
	t1.flags = 0;
	m_triangles[t1_index] = t1;

	m_num_triangles++;

	t2.vertex[0] = target.vertex[2];
	t2.vertex[1] = target.vertex[0];
	t2.vertex[2] = v_index;
	t2.neighbor[0] = t0_index;
	t2.neighbor[1] = t1_index;
	t2.neighbor[2] = target.neighbor[1];
	t2.flags = 0;
	m_triangles[t2_index] = t2;

	m_num_triangles++;

	// update neighborhood:
	if (target.neighbor[0] != -1)
		for (int k = 0; k<3; ++k)
			if (m_triangles[target.neighbor[0]].neighbor[k] == i)
			{
				m_triangles[target.neighbor[0]].neighbor[k] = t1_index;
				break;
			}
	if (target.neighbor[1] != -1)
		for (int k = 0; k<3; ++k)
			if (m_triangles[target.neighbor[1]].neighbor[k] == i)
			{
				m_triangles[target.neighbor[1]].neighbor[k] = t2_index;
				break;
			}

	// Update vertices incidences:
	m_vertices[v_index].triangle = t0_index;
	m_vertices[target.vertex[2]].triangle = t2_index;
	/*m_vertices[v_index].addIncidentTriangle(t0_index);
	m_vertices[v_index].addIncidentTriangle(t1_index);
	m_vertices[v_index].addIncidentTriangle(t2_index);
	m_vertices[target.vertex[0]].addIncidentTriangle(t2_index);
	m_vertices[target.vertex[1]].addIncidentTriangle(t1_index);
	m_vertices[target.vertex[2]].replaceIndicentTriangle(t0_index, t2_index);
	m_vertices[target.vertex[2]].addIncidentTriangle(t1_index);*/

	return v_index;
}

int SphericalDelaunay::insertVertex(const math::dvec3 & vertex)
{
	int target_triangle_index = smartFindTriangleThatContains(vertex);
	//if (target_triangle_index == -1)
		//return -1;
	
	return insertVertexAtTriangle(vertex, target_triangle_index);
}



void SphericalDelaunay::createNaiveTriangulation(const std::vector<math::dvec3> & vertices)
{
	std::cout << " ... inserting vertices naively" << std::endl;
	for (math::dvec3 v : vertices)
	{	
		int i = smartFindTriangleThatContains(v);		
		naiveInsertVertexAtTriangle(v, i);		
	}
}

void SphericalDelaunay::applyLawson()
{
	// Edges construction:
	std::cout << "  (building edges...";
	std::unordered_map<uint64_t, SphericalEdge> edges;// constant-time-access map
	for (int i = 0; i < m_triangle_index; ++i)
	{
		if (TRIANGLE_DELETED(m_triangles[i]))
			continue;

		int v0 = m_triangles[i].vertex[0];
		int v1 = m_triangles[i].vertex[1];
		int v2 = m_triangles[i].vertex[2];

		uint64_t akey01 = hashEdge(v0, v1);
		uint64_t akey02 = hashEdge(v0, v2);
		uint64_t akey12 = hashEdge(v1, v2);

		auto it = edges.find(akey01);
		if (it == edges.end())
		{
			SphericalEdge edge01 = { v0, v1, i, -1 };
			edges[akey01] = edge01;
		}
		else
			it->second.triangle[1] = i;

		it = edges.find(akey02);
		if (it == edges.end())
		{
			SphericalEdge edge02 = { v0, v2, i, -1 };
			edges[akey02] = edge02;
		}
		else
			it->second.triangle[1] = i;

		it = edges.find(akey12);
		if (it == edges.end())
		{
			SphericalEdge edge12 = { v1, v2, i, -1 };
			edges[akey12] = edge12;
		}
		else
			it->second.triangle[1] = i;
	}
	//std::cout << " " << edges.size() << ")" << std::endl;

	// Edges scan - we store in a list those that need to be flipped (to garantee the Delaunay condition):
	std::list<uint64_t> flipList;
	for (auto it = edges.begin(); it != edges.end(); ++it)
	{
		if (it->second.triangle[0] == -1 || it->second.triangle[1] == -1)
			continue;
		if (!checkEdgeDelaunay(it->second))
			flipList.push_back(it->first);
	}

	//std::cout << "  (flipping edges...)" << std::endl;
	// Edges processing (LAWSON's algorithm) :	
	while (!flipList.empty())
	{
		uint64_t edgeKey = flipList.front();
		flipList.pop_front();

		auto e = edges.find(edgeKey);
		if (e == edges.end() || e->second.triangle[0] == -1 || e->second.triangle[1] == -1)
			continue;
		SphericalEdge edge = e->second;

		// Indices of vertices like so:
		//		v0, v1, v2 within the first triangle, v within the second one,
		//      v0 and v facing the edge
		//      v0, v1 and v2 in counterclockwise order
		int v, v0, v1, v2;
		flipEdge(edge, v, v0, v1, v2);

		// Edges update:
		edges.erase(edgeKey);

		SphericalEdge new_edge;
		new_edge.vertex[0] = v0;
		new_edge.vertex[1] = v;
		new_edge.triangle[0] = edge.triangle[0];
		new_edge.triangle[1] = edge.triangle[1];
		edges[hashEdge(v0, v)] = new_edge;

		SphericalEdge & e0_20 = edges.find(hashEdge(v2, v0))->second;
		if (e0_20.triangle[0] == edge.triangle[0])
			e0_20.triangle[0] = edge.triangle[1];
		else if (e0_20.triangle[1] == edge.triangle[0])
			e0_20.triangle[1] = edge.triangle[1];

		SphericalEdge & e1_20 = edges.find(hashEdge(v1, v))->second;
		if (e1_20.triangle[0] == edge.triangle[1])
			e1_20.triangle[0] = edge.triangle[0];
		else if (e1_20.triangle[1] == edge.triangle[1])
			e1_20.triangle[1] = edge.triangle[0];

		// Eval of the 4 adjacent edges (around the flipped edge that is) and possibly add them to the Lawson list:
		uint64_t adjacents[4] = { hashEdge(v0, v1), hashEdge(v1, v), hashEdge(v, v2), hashEdge(v2, v0) };
		for (int k = 0; k < 4; ++k)
		{
			const SphericalEdge & adje = edges[adjacents[k]];
			if (adje.triangle[0] == -1 || adje.triangle[1] == -1)
				continue;
			if (!checkEdgeDelaunay(adje))
				flipList.push_back(adjacents[k]);
		}
	}
}

void SphericalDelaunay::flipEdge(const SphericalEdge & edge, int &v, int &v0, int &v1, int &v2)
{
	// The two incident triangles to the edge:
	SphericalTriangle t0 = m_triangles[edge.triangle[0]];
	SphericalTriangle t1 = m_triangles[edge.triangle[1]];

	int i;
	for (i = 0; i < 3; ++i)
	{
		if (t0.vertex[i] == edge.vertex[0] || t0.vertex[i] == edge.vertex[1])
			continue;
		break;
	}
	v0 = t0.vertex[i];
	v1 = t0.vertex[(i + 1) % 3];
	v2 = t0.vertex[(i + 2) % 3];
	int j;
	for (j = 0; j < 3; ++j)
	{
		if (t1.vertex[j] == edge.vertex[0] || t1.vertex[j] == edge.vertex[1])
			continue;
		break;
	}
	v = t1.vertex[j];

	// Create two new triangles (in place of the two old ones):
	SphericalTriangle & new_t0 = m_triangles[edge.triangle[0]];
	SphericalTriangle & new_t1 = m_triangles[edge.triangle[1]];

	new_t0.vertex[0] = v0;
	new_t0.vertex[1] = v1;
	new_t0.vertex[2] = v;

	new_t1.vertex[0] = v0;
	new_t1.vertex[1] = v;
	new_t1.vertex[2] = v2;

	// Don't forget adjacency relations update:
	new_t0.neighbor[0] = t1.neighbor[(j + 1) % 3];
	new_t0.neighbor[1] = edge.triangle[1];
	new_t0.neighbor[2] = t0.neighbor[(i + 2) % 3];

	new_t1.neighbor[0] = t1.neighbor[(j + 2) % 3];
	new_t1.neighbor[1] = t0.neighbor[(i + 1) % 3];
	new_t1.neighbor[2] = edge.triangle[0];

	if (t1.neighbor[(j + 1) % 3] != -1)//make sure this is not an edge of the triangulation (edge of the convex hull that is)
	{
		SphericalTriangle & neighbor00 = m_triangles[t1.neighbor[(j + 1) % 3]];
		for (int k = 0; k < 3; ++k)
			if (neighbor00.neighbor[k] == edge.triangle[1])
			{
				neighbor00.neighbor[k] = edge.triangle[0];
				break;
			}
	}
	if (t0.neighbor[(i + 1) % 3] != -1)
	{
		SphericalTriangle & neighbor11 = m_triangles[t0.neighbor[(i + 1) % 3]];
		for (int k = 0; k < 3; ++k)
			if (neighbor11.neighbor[k] == edge.triangle[0])
			{
				neighbor11.neighbor[k] = edge.triangle[1];
				break;
			}
	}

	m_vertices[v0].triangle = edge.triangle[0];
	m_vertices[v1].triangle = edge.triangle[0];
	m_vertices[v2].triangle = edge.triangle[1];
	m_vertices[v].triangle = edge.triangle[1];
	/*m_vertices[v0].addIncidentTriangle(edge.triangle[1]);
	m_vertices[v1].removeIncidentTriangle(edge.triangle[1]);
	m_vertices[v2].removeIncidentTriangle(edge.triangle[0]);
	m_vertices[v].addIncidentTriangle(edge.triangle[0]);*/
}

bool SphericalDelaunay::checkEdgeDelaunay(const SphericalEdge & edge) const
{
	const SphericalTriangle & t0 = m_triangles[edge.triangle[0]];
	const SphericalTriangle & t1 = m_triangles[edge.triangle[1]];

	// Indices of vertices like so:
	//		v0, v1, v2 within the first triangle, v within the second one,
	//      v0 and v facing the edge
	//      v0, v1 and v2 in counterclockwise order
	int i;
	for (i = 0; i < 3; ++i)
	{
		if (t0.vertex[i] == edge.vertex[0] || t0.vertex[i] == edge.vertex[1])
			continue;
		break;
	}
	int v0 = t0.vertex[i];
	int v1 = t0.vertex[(i + 1) % 3];
	int v2 = t0.vertex[(i + 2) % 3];
	for (i = 0; i < 3; ++i)
	{
		if (t1.vertex[i] == edge.vertex[0] || t1.vertex[i] == edge.vertex[1])
			continue;
		break;
	}
	int v = t1.vertex[i];

	// Compute the circumscribed circle to (v0, v1, v2)
	math::dvec3 p0 = m_vertices[v0].coordinates;
	math::dvec3 p1 = m_vertices[v1].coordinates;
	math::dvec3 p2 = m_vertices[v2].coordinates;
	math::dvec3 n = math::cross( p1 - p0 , p2 - p0);
	math::dvec3 c_center = m_sphere_radius * math::normalize(n);//center on the sphere of the circumscribed circle
	math::dvec3 OC = math::normalize(c_center - m_sphere_center);
	double c_normalized_radius = std::acos(math::dot(OC, math::normalize(p0 - m_sphere_center)));//pseudo radius of the circumscribed circle

																							 // Test if v falls within the circle:
	return  c_normalized_radius  <  std::acos(math::dot(OC, math::normalize(m_vertices[v].coordinates - m_sphere_center)));
}



int SphericalDelaunay::findTriangleThatContains(math::dvec3 p) const
{
	for (int i = 0; i < m_triangle_index; ++i)
	{
		if (!TRIANGLE_DELETED(m_triangles[i]))
		{
			if (m_triangles[i].containsPoint(m_vertices, m_sphere_center, p))
				return i;
		}		
	}
	return -1;
}

double SphericalDelaunay::getAverageTriangleEdgeLength() const
{
	double edge = 0.0;
	double count = 0.0;
	for (int t = 0; t < m_triangle_index; ++t)
	{
		if (TRIANGLE_DELETED(m_triangles[t]))
			continue;
		math::dvec3 e0 = m_vertices[m_triangles[t].vertex[1]].coordinates - m_vertices[m_triangles[t].vertex[0]].coordinates;
		math::dvec3 e1 = m_vertices[m_triangles[t].vertex[2]].coordinates - m_vertices[m_triangles[t].vertex[1]].coordinates;
		math::dvec3 e2 = m_vertices[m_triangles[t].vertex[0]].coordinates - m_vertices[m_triangles[t].vertex[2]].coordinates;
		edge += math::length(e0) + math::length(e1) + math::length(e2);
		count += 3.0;
	}
	return edge/count;
}


int SphericalDelaunay::smartFindTriangleThatContains(math::dvec3 p) const
{
	//std::set<int> visited;
	int t_index;
	do {
		t_index = rand() % m_triangle_index;// pick-up a random starting triangle
	} while (TRIANGLE_DELETED(m_triangles[t_index]));
	
	while (true)
	{
		//visited.insert(t_index);
		const SphericalTriangle & t = m_triangles[t_index];

		// look-up the three vertices coordinates:
		math::dvec3 v0 = m_vertices[t.vertex[0]].coordinates;
		math::dvec3 v1 = m_vertices[t.vertex[1]].coordinates;
		math::dvec3 v2 = m_vertices[t.vertex[2]].coordinates;

		if (math::dot(v0 - m_sphere_center, p - m_sphere_center) < 0.0)// quick test : if p is in the opposite hemisphere with respect to the current triangle...
		{
			do {
				t_index = rand() % m_triangle_index;// ... then pick-up a new random triangle
			} while (TRIANGLE_DELETED(m_triangles[t_index]));
			continue;
		}

		/*tool::Triangle T(v0, v1, v2);
		tool::Ray R(m_sphere_center, math::normalize(p - m_sphere_center));		
		double ht, u, w;
		if (T.intersect(R, DBL_MAX, ht, u, w))
			return t_index;*/

		// compute the three normals:
		math::dvec3 O0 = math::normalize(v0 - m_sphere_center);
		math::dvec3 O1 = math::normalize(v1 - m_sphere_center);
		math::dvec3 O2 = math::normalize(v2 - m_sphere_center);

		math::dvec3 n0 = math::cross(O1, O2); 
		math::dvec3 n1 = math::cross(O2, O0);
		math::dvec3 n2 = math::cross(O0, O1);
		
		double d0 = math::dot(math::normalize(p - v1), n0);
		double d1 = math::dot(math::normalize(p - v2), n1);
		double d2 = math::dot(math::normalize(p - v0), n2);
		bool test0 = (d0 >= EPSILON_TEST);
		bool test1 = (d1 >= EPSILON_TEST);
		bool test2 = (d2 >= EPSILON_TEST);
		if (test0 && test1 && test2)
			return t_index;//we found the containing triangle, return.

		// Else move to a neighboring triangle in the right direction
		if (!test0 && t.neighbor[0] != -1)
			t_index = t.neighbor[0];
		else if (!test1 && t.neighbor[1] != -1)
			t_index = t.neighbor[1];
		else if (!test2 && t.neighbor[2] != -1)
			t_index = t.neighbor[2];
		else do {
			t_index = rand() % m_triangle_index;
		} while (TRIANGLE_DELETED(m_triangles[t_index]));
	}

	return -1;
}



void SphericalDelaunay::makeBaseOctahedron()
{
	// 0 : OH apex north
	double x = m_sphere_center[0];
	double y = m_sphere_center[1];
	double z = m_sphere_center[2] + m_sphere_radius;
	m_vertices[0].coordinates = math::dvec3(x, y, z);
	m_vertices[0].triangle = 0;
	/*for (int i = 0; i < MAX_INCIDENT_TRIANGLES; ++i)
		m_vertices[0].triangle[i] = -1;
	m_vertices[0].triangle[0] = 0;
	m_vertices[0].triangle[1] = 2;
	m_vertices[0].triangle[2] = 4;
	m_vertices[0].triangle[3] = 6;*/
	m_vertices[0].id = 0;

	// 1 : OH apex south
	x = m_sphere_center[0];
	y = m_sphere_center[1];
	z = m_sphere_center[2] - m_sphere_radius;
	m_vertices[1].coordinates = math::dvec3(x, y, z);
	m_vertices[1].triangle = 1;
	/*for (int i = 0; i < MAX_INCIDENT_TRIANGLES; ++i)
		m_vertices[0].triangle[i] = -1;
	m_vertices[1].triangle[0] = 1;
	m_vertices[1].triangle[1] = 3;
	m_vertices[1].triangle[2] = 5;
	m_vertices[1].triangle[3] = 7;*/
	m_vertices[1].id = 1;

	// 2 : OH base square, left
	x = m_sphere_center[0] - m_sphere_radius;
	y = m_sphere_center[1];
	z = m_sphere_center[2];
	m_vertices[2].coordinates = math::dvec3(x, y, z);
	m_vertices[2].triangle = 0;
	/*for (int i = 0; i < MAX_INCIDENT_TRIANGLES; ++i)
		m_vertices[0].triangle[i] = -1;
	m_vertices[2].triangle[0] = 0;
	m_vertices[2].triangle[1] = 1;
	m_vertices[2].triangle[2] = 6;
	m_vertices[2].triangle[3] = 7;*/

	m_vertices[2].id = 2;

	// 3 : OH base square, bottom 
	x = m_sphere_center[0] ;
	y = m_sphere_center[1] - m_sphere_radius;
	z = m_sphere_center[2];
	m_vertices[3].coordinates = math::dvec3(x, y, z);
	m_vertices[3].triangle = 0;
	/*for (int i = 0; i < MAX_INCIDENT_TRIANGLES; ++i)
		m_vertices[0].triangle[i] = -1;
	m_vertices[3].triangle[0] = 0;
	m_vertices[3].triangle[1] = 1;
	m_vertices[3].triangle[2] = 2;
	m_vertices[3].triangle[3] = 3;*/
	m_vertices[3].id = 3;

	// 4 : OH base square, right
	x = m_sphere_center[0] + m_sphere_radius;
	y = m_sphere_center[1];
	z = m_sphere_center[2];
	m_vertices[4].coordinates = math::dvec3(x, y, z);
	m_vertices[4].triangle = 2;
	/*for (int i = 0; i < MAX_INCIDENT_TRIANGLES; ++i)
		m_vertices[0].triangle[i] = -1;
	m_vertices[4].triangle[0] = 2;
	m_vertices[4].triangle[1] = 3;
	m_vertices[4].triangle[2] = 4;
	m_vertices[4].triangle[3] = 5;*/
	m_vertices[4].id = 4;

	// 5 : OH base square, top
	x = m_sphere_center[0];
	y = m_sphere_center[1]+ m_sphere_radius;
	z = m_sphere_center[2];
	m_vertices[5].coordinates = math::dvec3(x, y, z);
	m_vertices[5].triangle = 4;
	/*for (int i = 0; i < MAX_INCIDENT_TRIANGLES; ++i)
		m_vertices[0].triangle[i] = -1;
	m_vertices[5].triangle[0] = 4;
	m_vertices[5].triangle[1] = 5;
	m_vertices[5].triangle[2] = 6;
	m_vertices[5].triangle[3] = 7;*/
	m_vertices[5].id = 5;

	m_num_vertices = 6;
	m_vertex_index = 6;
	m_vertex_id_count = 6;

	// 4 north+south octahedron equilateral triangles:
	for (int i = 0; i < 4; ++i)
	{
		// north:
		SphericalTriangle n;
		n.vertex[0] = 0;
		n.vertex[1] = 2 + i;
		n.vertex[2] = 2 + (i + 1) % 4;
		n.neighbor[0] = 2 * i + 1;// southern neighbor
		n.neighbor[1] = (2 * i + 2) % 8;
		n.neighbor[2] = (2 * i + 6) % 8;
		n.id = 2 * i;
		m_triangles[2 * i + 0] = n;

		// south:
		SphericalTriangle s;
		s.vertex[0] = 1;
		s.vertex[1] = 2 + (i + 1) % 4;
		s.vertex[2] = 2 + i;
		s.neighbor[0] = 2 * i;// northern neighbor
		s.neighbor[1] = (2 * i + 7) % 8;
		s.neighbor[2] = (2 * i + 3) % 8;
		s.id = 2 * i + 1;
		m_triangles[2 * i + 1] = s;
	}

	m_num_triangles = 8;
	m_triangle_index = 8;
	m_triangle_id_count = 8;
}

int SphericalDelaunay::findAvailableTriangleSlot()
{
	int slot;
	if (m_triangles_freeslots.empty())
	{
		slot = m_triangle_index;		
		m_triangle_index++;
		if (m_triangle_index >= m_triangle_capacity)
		{
			SphericalTriangle * tmp = new SphericalTriangle[2 * m_triangle_capacity];
			for (int i = 0; i < m_triangle_capacity; ++i)
				tmp[i] = m_triangles[i];
			delete[] m_triangles;
			m_triangles = tmp;
			m_triangle_capacity *= 2;
		}		
		return slot;
	}
	
	slot = m_triangles_freeslots[m_triangles_freeslots.size() - 1];
	m_triangles_freeslots.pop_back();
	return slot;
}

int SphericalDelaunay::findAvailableVertexSlot()
{
	int slot;
	if (m_vertices_freeslots.empty())
	{
		slot = m_vertex_index;
		m_vertex_index++;
		if (m_vertex_index >= m_vertex_capacity)
		{
			SphericalVertex * tmp = new SphericalVertex[2 * m_vertex_capacity];
			for (int i = 0; i < m_vertex_capacity; ++i)
				tmp[i] = m_vertices[i];
			delete[] m_vertices;
			m_vertices = tmp;
			m_vertex_capacity *= 2;
		}		
		return slot;
	}

	slot = m_vertices_freeslots[m_vertices_freeslots.size() - 1];
	m_vertices_freeslots.pop_back();
	return slot;
}

bool SphericalDelaunay::loadFromDisk(std::string&  filename)
{
	cleanup();

	std::cout << "Loading Spherical Delaunay Triangulation from disk (" << filename << ")... " << std::endl;
	std::ifstream file(filename.c_str(), std::ifstream::in | std::ifstream::binary);
	if (!file.good())
	{
		std::cout << "ERROR - failed opening " << filename << std::endl;
		return false;
	}		

	file.read((char*)&m_sphere_radius, sizeof(double));
	file.read((char*)&m_sphere_center, sizeof(math::dvec3));

	file.read((char*)&m_num_vertices, sizeof(int));
	file.read((char*)&m_num_triangles, sizeof(int));

	file.read((char*)&m_vertex_index, sizeof(int));
	file.read((char*)&m_triangle_index, sizeof(int));

	m_vertices = new SphericalVertex[2 * m_vertex_index];
	m_triangles = new SphericalTriangle[2 * m_triangle_index];
	m_triangle_capacity = 2 * m_triangle_index;
	m_vertex_capacity = 2 * m_vertex_index;

	for (int i = 0; i < m_vertex_index; ++i)
		file.read((char*)(m_vertices + i), sizeof(SphericalVertex));//m_vertices[i].load(file);

	for (int i = 0; i < m_triangle_index; ++i)
		file.read((char*)(m_triangles + i), sizeof(SphericalTriangle));

	file.close();

	m_vertex_id_count = m_vertex_index;
	m_triangle_id_count = m_triangle_index;

	return true;
}

bool SphericalDelaunay::persistToDisk(std::string & filename)
{
	std::cout << "Persisting Spherical Delaunay Triangulation to disk (" << filename << ")... " << std::endl;
	std::ofstream file(filename.c_str(), std::ofstream::out | std::ofstream::binary);
	if (!file.good())
	{
		std::cout << "ERROR - failed opening " << filename << std::endl;
		return false;
	}
	
	file.write((char*)&m_sphere_radius, sizeof(double));//write sphere radius
	file.write((char*)&m_sphere_center, sizeof(math::dvec3));//write sphere center

	file.write((char*)&m_num_vertices, sizeof(int));
	file.write((char*)&m_num_triangles, sizeof(int));

	file.write((char*)&m_vertex_index, sizeof(int));// write vertex range
	file.write((char*)&m_triangle_index, sizeof(int));// write triangle range

	for (int i = 0; i < m_vertex_index; ++i)// write all vertices
		file.write((char*)(m_vertices + i), sizeof(SphericalVertex));//m_vertices[i].persist(file);
	
	for (int i = 0; i < m_triangle_index; ++i)// write all triangles (even those deleted)
		file.write((char*)(m_triangles + i), sizeof(SphericalTriangle));

	file.close();
	return true;
}




