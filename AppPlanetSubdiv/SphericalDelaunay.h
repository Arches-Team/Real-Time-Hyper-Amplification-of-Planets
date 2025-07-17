#pragma once

#include "tool.h"
#include "PoissonSphereSampling.h"

#include <cassert>
#include <vector>
#include <list>
#include <set>
#include <fstream>
#include <iostream>


#define EPSILON_TEST  0.0
#define MAX_PER_VERTEX_TRIANGLE_INCIDENCE		16	// large enough


//Fwdcl:
struct SphericalVertex;


struct SphericalTriangle
{
	int vertex[3];/// indices of the 3 vertices
	int neighbor[3];/// indices of the 3 neighboring triangles (eg. neighbor[0] faces vertex[0])	
	unsigned int flags = 0;/// (internal) 
	int id = -1;
	
	/**
	 * @param vertices The list of vertices to look-up.
	 * @param v The point to test.
	 * @returns True if v is inside the spherical triangle.
	 * Implementation : 
	 * The 3 edges of the triangle define 3 planes passing through the center of the sphere.
	 * A point v is inside the triangle if and only if v is on the same side with respect to the 3 planes.
	 */
	bool containsPoint(const SphericalVertex * vertices, const math::dvec3 & sphere_center, const math::dvec3 & v) const;//FIXME : move this function into SphericalDelaunay class

	inline bool isBoundary() const {
		return neighbor[0] == -1 || neighbor[1] == -1 || neighbor[2] == -1;
	}

	inline void remove() { flags |= 1; }	
};
#define TRIANGLE_DELETED(t) (t.flags & 1 != 0)


struct SphericalEdge
{
	int vertex[2];/// indices of the 2 vertices
	int triangle[2];/// indices of the 2 neighboring triangles
};


struct SphericalVertex
{
	math::dvec3 coordinates;/// 3D coordinates
	int triangle;/// An incident triangle index (one among many)	
	unsigned int flags = 0;/// (internal)	
	int id = -1;		

	void remove() { flags |= 1; }

	/**
	 * @param vertex_index The index of the current vertex.
	 * @param triangles The list of all triangles of the triangulation
	 * @param [out] incident The returned set of incident triangles to this vertex.
	*/
	void getIncidentTriangles(int vertex_index, const SphericalTriangle * triangles, std::set<int> & incident) const;
};
#define VERTEX_DELETED(v) (v.flags & 1 != 0)


/**
 * @brief Spherical Delaunay Triangulation.
 * The triangulation uses an initial set of triangles to overcome problems with the sphere. In the implementation this initial set is comprised of the 8 triangles of a regular octahedron.
 */
class SphericalDelaunay
{


public:
	
	/**
	 * Default constructor (should not be called by application).
	 */
	inline SphericalDelaunay() { }

	/**
	 * Constructor.
	 * @param sphere_radius Radius of the sphere to triangulate.
	 * @param reserve (optional) Approximate number of vertices, which the code can use to pre-allocate memory and improve performance.
	 */
	explicit SphericalDelaunay(double sphere_radius, unsigned int reserve = 0);

	/**
	 * Constructor.
	 * @brief constructs a spherical delaunay triangulation from poisson sampling of the sphere.
	 */
	SphericalDelaunay(double sphere_radius, double poisson_radius);

	/**
	* Constructor.
	* @param vertices An initial list of vertices to build upon (note that the initial octahedron vertices will still be part of the triangulation).
	* @param sphere_radius (optional) Radius of the sphere to triangulate.	
	*/
	//explicit SphericalDelaunay(const std::vector<math::dvec3> & vertices, double sphere_radius = 1.0);

	/**
	 * Copy ctor.
	 */
	SphericalDelaunay(const SphericalDelaunay & sdt);

	virtual ~SphericalDelaunay() { cleanup(); }
	
	/** Copy assignment operator. */
	SphericalDelaunay & operator=(const SphericalDelaunay & sdt);



	/** Load instance from disk */
	bool loadFromDisk(std::string &filename);

	/** Save instance data to disk. */
	bool persistToDisk(std::string &filename);

	/**
	 * Releases resources.
	 */
	virtual void cleanup();
		
	/** Rescale the sphere. */
	void changeSphereRadius(double new_radius);

	/**
	 * Inserts a vertex into the triangulation, preserving the Delaunay property.
	 * @returns The index of the inserted vertex. 
	 */
	int insertVertex(const math::dvec3 & vertex);
	/**
	* Inserts a vertex into the triangulation, at a specified triangle, preserving the Delaunay property.
	* @returns The index of the inserted vertex.
	*/
	int insertVertexAtTriangle(const math::dvec3 & vertex, int triangle_index);
	/**
	 * Naïvely inserts a vertex into the specified triangle (the Delaunay property is not garanteed any more after this operation).
	 */
	int naiveInsertVertexAtTriangle(const math::dvec3 & vertex, int triangle_index);

	int naiveInsertVertex(const math::dvec3 & vertex);


	/** Applies the Lawson algorithm (adapted to the sphere), ie. converts a naive triangulation into a Delaunay one, using edge flips. */
	void applyLawson();

	inline int nextFreeVertexID() { return m_vertex_id_count++; }

	inline SphericalVertex & getVertex(int index) { assert(index < m_vertex_index); return m_vertices[index]; }
	
	inline const SphericalVertex * getVertices() const { return m_vertices; }
	
	inline int getNumVertices() const { return m_num_vertices; }
	
	inline int getVerticesRange() const { return m_vertex_index; }

	inline SphericalTriangle & getTriangle(int index) { assert(index < m_triangle_index);  return m_triangles[index]; }
	
	inline const SphericalTriangle * getTriangles() const { return m_triangles; }
	
	inline int getNumTriangles() const { return m_num_triangles; }
	
	inline int getTrianglesRange() const { return m_triangle_index; }
	
	/**
	 * @returns A unique hashed value, from a non ordered pair of vertex indices.
	 */
	inline uint64_t hashEdge(unsigned int s0, unsigned int s1)
	{
		uint64_t m = s0; uint64_t M = s1;
		if (s1 < s0) {	m = s1;	M = s0;	}		
		return (M << 32) | m;
	}
	/**
	* @returns A unique hashed value from the specified edge.
	*/
	inline uint64_t hashEdge(const SphericalEdge & edge) { return hashEdge(edge.vertex[0], edge.vertex[1]); }	

	double getSphereRadius() const { return m_sphere_radius; }

	/** @returns The index of the containing triangle,or -1 if no triangle contains p.*/
	int findTriangleThatContains(math::dvec3 p) const;

	/** Average edge length in km */
	double getAverageTriangleEdgeLength() const;


protected:

	/** @brief Flips an edge. 
	 *  Indices of vertices like so:
	 *	v0, v1, v2 within the first triangle, v within the second one,
	 *	v0 and v facing the edge
	 *	v0, v1 and v2 in counterclockwise order
	 */
	void flipEdge(const SphericalEdge & edge, int &v, int &v0, int &v1, int &v2);
	/** @returns True if the edge respects the Delaunay condition */
	bool checkEdgeDelaunay(const SphericalEdge & edge) const;
	/** Builds the initial octahedron (ie. the 8 base triangles on the sphere)*/
	void makeBaseOctahedron();
	/** @returns The next free index in the triangles array */
	int findAvailableTriangleSlot();
	/** @returns The next free index in the vertices array */
	int findAvailableVertexSlot();
	/** Creates a naive triangulation (ie. not of Delaunay) from a list of vertices, inserted into the base octahedron. */
	void createNaiveTriangulation(const std::vector<math::dvec3> & vertices);
	/** Finds the triangle that contains the specified point, using triangle marching by general direction heuristic (important: the point p is assumed to lie within the triangulation). */
	int smartFindTriangleThatContains(math::dvec3 p) const;
	

protected:

	/// Big array storing all triangles.
	SphericalTriangle * m_triangles = nullptr;
	/// Big array storing all vertices.
	SphericalVertex * m_vertices = nullptr;	
	/// a stack that tracks next available slot in vertices array
	std::vector<int> m_vertices_freeslots;
	/// a stack that tracks next available slot in triangles array
	std::vector<int> m_triangles_freeslots;
	/// total number of vertices
	unsigned int m_num_vertices = 0;
	/// first available index in the vertex array
	unsigned int m_vertex_index = 0;
	/// allocated size of the vertex array
	unsigned int m_vertex_capacity = 0;
	/// total number of triangles
	unsigned int m_num_triangles = 0;
	/// first available index in the triangle array
	unsigned int m_triangle_index = 0;
	///allocated size of the triangle array
	unsigned int m_triangle_capacity = 0;
	/// [deprecated] for now we default this attribute to (0,0,0) without any means to change it.
	math::dvec3 m_sphere_center;
	/// The sphere radius.
	double m_sphere_radius = 1.0;
	int m_vertex_id_count = 0;
	int m_triangle_id_count = 0;//(unused)
};
