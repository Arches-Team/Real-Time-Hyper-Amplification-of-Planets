#pragma once

#include "glversion.h"

#define GLM_FORCE_UNRESTRICTED_GENTYPE
#define GLM_FORCE_CXX11
#define GLM_FORCE_SIMD_AVX
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp" // glm::translate, glm::rotate, glm::scale, glm::perspective
#include "glm/gtc/type_ptr.hpp" // value_ptr


#define PI	3.1415926535897932384626433832795


namespace math = glm;


namespace tool
{

	// construit un repere ortho tbn, a partir d'un seul vecteur...
	// cf "generating a consistently oriented tangent space" 
	// http://people.compute.dtu.dk/jerf/papers/abstracts/onb.html
	struct MonoVectorFrame
	{
		explicit MonoVectorFrame(const glm::dvec3& _n) : n(_n)
		{
			if (n[2] < -0.9999999)
			{
				t = glm::dvec3(0.0, -1.0, 0.0);
				b = glm::dvec3(-1.0, 0.0, 0.0);
			}
			else
			{
				double a = 1.0 / (1.0 + n.z);
				double d = -n.x * n.y * a;
				t = glm::dvec3(1.0 - n.x * n.x * a, d, -n.x);
				b = glm::dvec3(d, 1.0 - n.y * n.y * a, -n.y);
			}
		}

		glm::dvec3 t;
		glm::dvec3 b;
		glm::dvec3 n;
	};


	
	/// out : t0, t1 ray parameters to intersection. If no intersection found then t0 = -DBL_MAX and t1 = -DBL_MAX
	inline void rayIntersectSphere(const glm::dvec3 & sphereCenter, double sphereRadius, const glm::dvec3 & origin, const glm::dvec3 & direction, double & t0, double & t1)
	{
		glm::dvec3 cam = origin - sphereCenter;

		double A = glm::dot(direction, direction);
		double C = glm::dot(cam, cam) - sphereRadius * sphereRadius;
		double B = 2.0 * (cam[0] * direction[0] + cam[1] * direction[1] + cam[2] * direction[2]);
		double delta = B * B - 4.0*A*C;

		if (delta < 0.0) {
			t0 = -DBL_MAX;
			t1 = -DBL_MAX;
			return;
		}
		double iA = 1.0 / (2.0*A);
		double sq = std::sqrt(delta);
		t0 = (-B - sq) * iA;
		t1 = (-B + sq) * iA;
		return;
	}
}



namespace tool
{	
	struct Ray
	{
		glm::dvec3 o;  //!< origin.
		glm::dvec3 d; //!< direction.
		double tmax; //!< absciss max for valid intersections.

		//Ray(const glm::dvec3 origin, const glm::dvec3 end) : o(origin), d(end - origin), tmax(1.0) {}
		Ray(const glm::dvec3 origin, const glm::dvec3 direction) : o(origin), d(direction), tmax(DBL_MAX) {}

		//! renvoie le point a l'abscisse t sur le rayon
		glm::dvec3 operator( ) (const double t) const { return o + t * d; }
	};

	
	//! Axis Aligned Bounding Box
	struct AABB
	{
		glm::dvec3 pmin;
		glm::dvec3 pmax;

		/// d'après http://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
		bool intersectRay(const Ray & ray) const
		{
			double tmin, tmax, tymin, tymax, tzmin, tzmax;
			glm::dvec3 direction = glm::normalize(ray.d);

			double ix = 1.0 / direction[0];
			if (ix >= 0.0) {
				tmin = (pmin[0] - ray.o[0]) * ix;
				tmax = (pmax[0] - ray.o[0]) * ix;
			}
			else {
				tmin = (pmax[0] - ray.o[0]) * ix;
				tmax = (pmin[0] - ray.o[0]) * ix;
			}

			double iy = 1.0 / direction[1];
			if (iy >= 0.0) {
				tymin = (pmin[1] - ray.o[1]) * iy;
				tymax = (pmax[1] - ray.o[1]) * iy;
			}
			else {
				tymin = (pmax[1] - ray.o[1]) * iy;
				tymax = (pmin[1] - ray.o[1]) * iy;
			}

			if ((tmin > tymax) || (tymin > tmax))
				return false;

			if (tymin > tmin)
				tmin = tymin;

			if (tymax < tmax)
				tmax = tymax;

			double iz = 1.0 / direction[2];
			if (iz >= 0.0) {
				tzmin = (pmin[2] - ray.o[2]) * iz;
				tzmax = (pmax[2] - ray.o[2]) * iz;
			}
			else {
				tzmin = (pmax[2] - ray.o[2]) * iz;
				tzmax = (pmin[2] - ray.o[2]) * iz;
			}

			if ((tmin > tzmax) || (tzmin > tmax))
				return false;

			return true;
		}
	};


	struct Triangle 
	{
		//! Intersection point within a triangle.
		struct Intersection
		{
			glm::dvec3 p;      //!< position.
			glm::dvec3 n;     //!< normal.
			double t;      //!< t, absciss along the ray.
			double u, v;     //!< u, v barycentric coordinates
			int ref;  //! internal.

			Intersection() : p(), n(), t(DBL_MAX), u(0), v(0), ref(-1) {}
		};

		const double epsilon = 0.00001;
		glm::dvec3 vertex[3];//!< The three vertices.
		int ref;//!< Internal reference.
		Triangle() {}
		Triangle(const glm::dvec3 &a, const glm::dvec3 &b, const glm::dvec3 & c) {
			vertex[0] = a;
			vertex[1] = b;
			vertex[2] = c;
		}

		Triangle & operator=(const Triangle & t)
		{
			vertex[0] = t.vertex[0];
			vertex[1] = t.vertex[1];
			vertex[2] = t.vertex[2];
			ref = t.ref;
			return *this;
		}
		
		/* calcule l'intersection ray/triangle
		cf "fast, minimum storage ray-triangle intersection"
		http://www.graphics.cornell.edu/pubs/1997/MT97.pdf

		renvoie faux s'il n'y a pas d'intersection valide, une intersection peut exister mais peut ne pas se trouver dans l'intervalle [0 htmax] du rayon. \n
		renvoie vrai + les coordonnees barycentriques (ru, rv) du point d'intersection + sa position le long du rayon (rt). \n
		convention barycentrique : t(u, v)= (1 - u - v) * a + u * b + v * c \n
		*/
		bool intersect(const Ray &ray, const double htmax, double &rt, double &ru, double &rv) const
		{
			/* begin calculating determinant - also used to calculate U parameter */
			glm::dvec3 ac = vertex[2] - vertex[0];
			glm::dvec3 pvec = glm::cross(ray.d, ac);//cross product

			/* if determinant is near zero, ray lies in plane of triangle */
			glm::dvec3 ab = vertex[1] - vertex[0];
			double det = glm::dot(ab, pvec);
			if (det > -epsilon && det < epsilon)
				return false;

			double inv_det = 1.0 / det;

			/* calculate distance from vert0 to ray origin */
			glm::dvec3 tvec(ray.o - vertex[0]);

			/* calculate U parameter and test bounds */
			double u = glm::dot(tvec, pvec) * inv_det;
			if (u < 0.0 || u > 1.0)
				return false;

			/* prepare to test V parameter */
			glm::dvec3 qvec = glm::cross(tvec, ab);

			/* calculate V parameter and test bounds */
			double v = glm::dot(ray.d, qvec) * inv_det;
			if (v < 0.0 || u + v > 1.0)
				return false;

			/* calculate t, ray intersects triangle */
			rt = glm::dot(ac, qvec) * inv_det;
			ru = u;
			rv = v;

			// ne renvoie vrai que si l'intersection est valide (comprise entre tmin et tmax du rayon)
			return (rt <= htmax && rt > epsilon);
		}

		//! renvoie l'aire du triangle
		double area() const
		{
			return 0.5 * glm::length(glm::cross(vertex[1] - vertex[0], vertex[2] - vertex[0]));
		}

		//! renvoie un point a l'interieur du triangle connaissant ses coordonnees barycentriques.
		//! convention p(u, v)= (1 - u - v) * a + u * b + v * c
		glm::dvec3 lerp(const double u, const double v) const
		{
			double w = 1.0 - u - v;
			return vertex[0] * w + vertex[1] * u + vertex[2] * v;
		}
	};




	//!< A Bounding Volumes Hierarchy over planar triangles.
	class BVH
	{
	public:
		explicit BVH(const std::vector<tool::Triangle> & _triangles) : m_triangles(_triangles.begin(), _triangles.end())
		{
			m_rootIndex = build_node(0, m_triangles.size());
		}
		~BVH()
		{}

		bool intersectRay(const tool::Ray & ray, std::vector<tool::Triangle::Intersection> & hitlist) const
		{
			return nodeIntersectRay(m_rootIndex, ray, hitlist);
		}

	private:
		struct BVHNode
		{
			tool::AABB boundingBox;
			int fils_g = -1, fils_d = -1;// indices des noeuds (dans le vecteur de BVHNode du BVH)
			int start = -1;// indice de début des triangles de la feuille (note:si le noeud est une feuille alors cette valeur est positive)
			int end = -1; // indice de fin des triangles de la feuille
		};

		struct predicatCut
		{
			int axis;
			double cut;

			predicatCut(int _axis, double _cut) : axis(_axis), cut(_cut) {}
			bool operator() (const tool::Triangle& t) const
			{
				double pos;
				switch (axis)
				{
				case 0: pos = (t.vertex[0][0] + t.vertex[1][0] + t.vertex[2][0]) / 3.0; break;
				case 1: pos = (t.vertex[0][1] + t.vertex[1][1] + t.vertex[2][1]) / 3.0; break;
				case 2: pos = (t.vertex[0][2] + t.vertex[1][2] + t.vertex[2][2]) / 3.0; break;
				default: break;
				}
				return pos < cut;
			}
		};

		std::vector<tool::Triangle> m_triangles;
		std::vector<BVHNode> m_nodes;
		int m_rootIndex;


	private:
		int partition(unsigned int start, unsigned int end, const predicatCut & predicat)
		{
			while (start < end)
			{
				if (!predicat(m_triangles[start]))
				{
					std::swap(m_triangles[start], m_triangles[end - 1]);
					--end;
				}
				else
					++start;
			}
			return start;
		}

		bool nodeIntersectRay(unsigned int node_index, const tool::Ray & ray, std::vector<tool::Triangle::Intersection> & hitlist) const
		{
			const BVHNode & node = m_nodes[node_index];
			if (!node.boundingBox.intersectRay(ray))
				return false;

			if (node.start >= 0)// cas feuille:
			{
				double t, u, v;
				tool::Triangle::Intersection h;
				bool found = false;
				for (int i = node.start; i < node.end; ++i)
					if (m_triangles[i].intersect(ray, ray.tmax, t, u, v))
					{
						h.t = t;
						h.u = u;
						h.v = v;
						h.p = ray(t);      // evalue la positon du point d'intersection sur le rayon					
						h.ref = m_triangles[i].ref; // permet de retrouver toutes les infos associees au triangle
						hitlist.push_back(h);
						found = true;
					}
				return found;
			}

			// descente récursive de l'arbre:
			bool r1 = false;
			if (node.fils_g >= 0)
				r1 = nodeIntersectRay(node.fils_g, ray, hitlist);

			bool r2 = false;
			if (node.fils_d >= 0)
				r2 = nodeIntersectRay(node.fils_d, ray, hitlist);

			return r1 || r2;
		}

		tool::AABB createBoundingBox(unsigned int start, unsigned int end)
		{
			tool::AABB boundingBox;
			for (int i = 0; i <3; ++i)
			{
				boundingBox.pmin[i] = DBL_MAX;
				boundingBox.pmax[i] = -DBL_MAX;
			}
			for (unsigned int i = start; i < end; ++i)
			{
				const tool::Triangle & t = m_triangles[i];
				boundingBox.pmin[0] = std::min(boundingBox.pmin[0], std::min(std::min(t.vertex[0][0], t.vertex[1][0]), t.vertex[2][0]));
				boundingBox.pmin[1] = std::min(boundingBox.pmin[1], std::min(std::min(t.vertex[0][1], t.vertex[1][1]), t.vertex[2][1]));
				boundingBox.pmin[2] = std::min(boundingBox.pmin[2], std::min(std::min(t.vertex[0][2], t.vertex[1][2]), t.vertex[2][2]));
				boundingBox.pmax[0] = std::max(boundingBox.pmax[0], std::max(std::max(t.vertex[0][0], t.vertex[1][0]), t.vertex[2][0]));
				boundingBox.pmax[1] = std::max(boundingBox.pmax[1], std::max(std::max(t.vertex[0][1], t.vertex[1][1]), t.vertex[2][1]));
				boundingBox.pmax[2] = std::max(boundingBox.pmax[2], std::max(std::max(t.vertex[0][2], t.vertex[1][2]), t.vertex[2][2]));
			}
			return boundingBox;
		}

		unsigned int build_node(unsigned int start, unsigned int end)
		{
			BVHNode node;
			if (start >= end)
				return -1;
			if (start + 1 == end) // cas d'arrêt: 1 seul triangle
			{
				const tool::Triangle & t = m_triangles[start];
				node.boundingBox.pmin[0] = std::min(std::min(t.vertex[0][0], t.vertex[1][0]), t.vertex[2][0]);
				node.boundingBox.pmin[1] = std::min(std::min(t.vertex[0][1], t.vertex[1][1]), t.vertex[2][1]);
				node.boundingBox.pmin[2] = std::min(std::min(t.vertex[0][2], t.vertex[1][2]), t.vertex[2][2]);
				node.boundingBox.pmax[0] = std::max(std::max(t.vertex[0][0], t.vertex[1][0]), t.vertex[2][0]);
				node.boundingBox.pmax[1] = std::max(std::max(t.vertex[0][1], t.vertex[1][1]), t.vertex[2][1]);
				node.boundingBox.pmax[2] = std::max(std::max(t.vertex[0][2], t.vertex[1][2]), t.vertex[2][2]);
				node.start = start;
				node.end = end;
				m_nodes.push_back(node);
				return m_nodes.size() - 1;
			}

			// Construire la bounding box des centroïdes des triangles:
			tool::AABB centroidAABB;
			for (int i = 0; i <3; ++i)
			{
				centroidAABB.pmin[i] = DBL_MAX;
				centroidAABB.pmax[i] = -DBL_MAX;
			}
			for (unsigned int i = start; i < end; ++i)
			{
				glm::dvec3 centroid = m_triangles[i].lerp(0.3333333333, 0.3333333333);
				centroidAABB.pmin[0] = std::min(centroidAABB.pmin[0], centroid[0]);
				centroidAABB.pmin[1] = std::min(centroidAABB.pmin[1], centroid[1]);
				centroidAABB.pmin[2] = std::min(centroidAABB.pmin[2], centroid[2]);
				centroidAABB.pmax[0] = std::max(centroidAABB.pmax[0], centroid[0]);
				centroidAABB.pmax[1] = std::max(centroidAABB.pmax[1], centroid[1]);
				centroidAABB.pmax[2] = std::max(centroidAABB.pmax[2], centroid[2]);
			}

			// Déterminer l'axe principal :
			double maxd = 0.0;
			int axis = 0;
			for (int i = 0; i < 3; ++i)
			{
				double d = centroidAABB.pmax[i] - centroidAABB.pmin[i];
				if (d > maxd)
				{
					maxd = d;
					axis = i;
				}
			}
			double cut = 0.5 * (centroidAABB.pmin[axis] + centroidAABB.pmax[axis]);

			// Partitionner les triangles :    
			unsigned int mid = partition(start, end, predicatCut(axis, cut));
			if (mid == start || mid == end)// prévenir les cas dégénérés où tous les triangles sont du même côté de la coupe:
			{
				for (int i = 0; i <2; ++i)
				{
					axis = (axis + 1) % 3;
					cut = 0.5 * (centroidAABB.pmin[axis] + centroidAABB.pmax[axis]);
					mid = partition(start, end, predicatCut(axis, cut));
					if (mid != start && mid != end)
						break;
				}
				if (mid == start || mid == end)// Cas: impossible de partitionner, donc on crée une feuille contenant tous ces triangles
				{
					//printf("Feuille à %d triangles\n", (int)(end - start)); 
					node.boundingBox = createBoundingBox(start, end);
					node.start = start;
					node.end = end;
					m_nodes.push_back(node);
					return m_nodes.size() - 1;
				}
			}

			// construire le fils gauche
			node.fils_g = build_node(start, mid);

			// construire le fils droit
			node.fils_d = build_node(mid, end);

			// Construire la bounding box effective:    
			node.boundingBox = createBoundingBox(start, end);

			// fin
			m_nodes.push_back(node);
			return m_nodes.size() - 1;
		}
	};
}




#include <QtGui/qvector4d.h>
#include <cmath>
#include <functional>
#include <chrono>
#include <random>



namespace glsl
{
	// Common vector structures, aligned in a GPU friendly manner.

	template < typename T >
	struct alignas(8) gvec2
	{
		alignas(4) T x, y;

		gvec2() {}
		gvec2(T x, T y) : x(x), y(y) {}
	};

	typedef gvec2<double> vec2;
	typedef gvec2<int> ivec2;
	typedef gvec2<unsigned int> uvec2;
	typedef gvec2<int> bvec2;

	template < typename T >
	struct alignas(16) gvec3
	{
		alignas(4) T x, y, z;

		gvec3() {}
		gvec3(T x, T y, T z) : x(x), y(y), z(z) {}
	};

	typedef gvec3<double> vec3;
	typedef gvec3<int> ivec3;
	typedef gvec3<unsigned int> uvec3;
	typedef gvec3<int> bvec3;

	template < typename T >
	struct alignas(16) gvec4
	{
		alignas(4) T x, y, z, w;

		gvec4() {}
		gvec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
	};

	typedef gvec4<double> vec4;
	typedef gvec4<int> ivec4;
	typedef gvec4<unsigned int> uvec4;
	typedef gvec4<int> bvec4;

}//end namespace glsl



namespace tool 
{
	
	/* PRNG */
	static std::mt19937 randgen = std::mt19937();// (randomseed);
	static std::uniform_real_distribution<double> randomDouble = std::uniform_real_distribution<double>(0.0, 1.0);

	/**
	* PERLIN 'SIMPLEX NOISE ADAPTED FROM :
	* Eliot Eshelman
	* Ashima Arts
	* 2, 3 and 4 Simplex Noise functions return 'random' values in [-1, 1].
	* 2, 3 and 4 fractal Noise return values in [0, 1].
	* This algorithm was originally designed by Ken Perlin,
	* see Ashima Arts and Stefan Gustavson for their work on webgl-noise.
	*/



	/**
	* @brief A weighted sum of simplex noises at a given 2D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 2d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalSimplex2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position);

	/**
	* @brief A weighted sum of abs(simplex noises) at a given 2D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 2d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalTurbulence2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position);

	/**
	* @brief A weighted sum of (1.0 - abs(simplex noises)) at a given 2D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 2d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalRidged2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position);

	/**
	* @brief A weighted sum of filter(simplex noise) at a given 2D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 2d point where the noise is evaluated.
	* @param filter a pointer to a unary filter, accepting values in [-1, 1] and returning a value in the same range - tip: use std::ptr_fun to encapsulate a function as a parameter.
	* @return a number in [0, 1]
	*/
	double fractalFilter2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position, std::pointer_to_unary_function<double, double> filter);

	/**
	* @brief A weighted sum of (1 - abs(filter(simplex noise))) at a given 2D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 2d point where the noise is evaluated.
	* @param filter a pointer to a unary filter, accepting values in [-1, 1] and returning a value in the same range - tip: use std::ptr_fun to encapsulate a function as a parameter.
	* @return a number in [0, 1]
	*/
	double fractalRidgedFilter2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position, std::pointer_to_unary_function<double, double> filter);

	/**
	* @brief Multifractal version of the weighted sum of abs(filter(simplex noise)) at a given 2D position : higher frequencies of noise are smoothed by previous noise values.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 2d point where the noise is evaluated.
	* @param filter a pointer to a unary filter, accepting values in [-1, 1] and returning a value in the same range - tip: use std::ptr_fun to encapsulate a function as a parameter.
	* @return a number in [0, 1]
	*/
	double multifractalTurbulenceFilter2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position, std::pointer_to_unary_function<double, double> filter);

	/**
	* @brief A weighted sum of simplex noises at a given 3D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 3d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalSimplex3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 &position);

	/**
	* @brief A weighted sum of abs(simplex noises) at a given 3D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 3d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalTurbulence3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 &position);

	/**
	* @brief A weighted sum of (1.0 - abs(simplex noises)) at a given 3D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 3d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalRidged3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 &position);


	/**
	* Worley Noise - minimal euclidian distance to feature points.
	* Returns a value in [0,1]
	*/
	double cellNoise3D(const glm::dvec3 & position);

	/**
	* FBM type cell noise.
	* Returns a value in [0,1]
	*/
	double fractalCellNoise3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 & position);

	/**
	* FBM type inverted cell noise. Each layer is 1.0 - cellNoise3D(...)
	* Returns a value in [0,1]
	*/
	double fractalInvertedCellNoise3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 & position);

	double fractalWarpedCellNoise3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 & position);

	double fractalInvertedWarpedCellNoise3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 & position);

	/**
	* Octaves of multiplied inverted cell noise.
	* Returns a value in [0,1]
	*/
	//double fractalMultInvertedCellNoise3D(int octaves, double frequency, const QVector3D & position);


	static std::mt19937 cellprng = std::mt19937();


}//end namespace tool



namespace tool
{

	class GeometryProxy
	{

	public:

		static void cleanup();

		inline static GLuint get_default_cube_vao()
		{
			if (!static_init)
				_init();
			return default_vao_cube;
		}

		inline static GLuint get_default_quad_vao()
		{
			if (!static_init)
				_init();
			return default_vao_quad;
		}

		inline static GLuint get_default_sphere_vao()
		{
			if (!static_init)
				_init();
			return default_vao_sphere;
		}

		inline static GLuint * get_default_cubemap_vao()
		{
			if (!static_init)
				_init();
			return default_vao_cubemap;
		}

		static void drawSphere();
		


	private:

		static bool static_init;
		static void _init();

		static float default_vertices_cube[144];
		static float default_vertices_quad[18];
		static float default_texcoords_quad[12];
		static GLuint default_vbo_cube, default_vbo_quad, default_vbo_sphere, cubevbo;
		static GLuint default_vao_cube, default_vao_quad, default_vao_sphere, cubevao;
		static GLuint default_vbo_cubemap[6];
		static GLuint default_vao_cubemap[6];
		static int num_sphere_triangles;
	};
}

