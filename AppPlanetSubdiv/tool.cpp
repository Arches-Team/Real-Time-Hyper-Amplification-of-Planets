#include "tool.h"







namespace tool {

	/** (tool) returns a precomputed templated gradient for the 3D case (simplex noise) - the two first components are also used for the 2D case.
	@param index must fit [0..15]
	*/
	inline static const double* gradient3D(int index)
	{
		// The 3d gradients are the midpoints of the vertices of a cube.
		static const double gradient[16 * 3] = {
			1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 1.0, -1.0, 0.0, -1.0, -1.0, 0.0,
			1.0, 0.0, 1.0, -1.0, 0.0, 1.0, 1.0, 0.0, -1.0, -1.0, 0.0, -1.0,
			0.0, 1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 1.0, -1.0, 0.0, -1.0, -1.0,
			1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 0.0, 1.0, -1.0, 0.0, -1.0, -1.0
		};
		return gradient + (index % 16);
	}

	/** (tool) returns a precomputed templated gradient for the 4D case (simplex noise) */
	inline static const double* gradient4D(int index)
	{
		// The 4d gradients are the midpoints of the vertices of a hypercube.
		static const double gradient[32 * 4] = {
			0.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, -1.0, 0.0, 1.0, -1.0, 1.0, 0.0, 1.0, -1.0, -1.0,
			0.0, -1.0, 1.0, 1.0, 0.0, -1.0, 1.0, -1.0, 0.0, -1.0, -1.0, 1.0, 0.0, -1.0, -1.0, -1.0,
			1.0, 0.0, 1.0, 1.0, 1.0, 0.0, 1.0, -1.0, 1.0, 0.0, -1.0, 1.0, 1.0, 0.0, -1.0, -1.0,
			-1.0, 0.0, 1.0, 1.0, -1.0, 0.0, 1.0, -1.0, -1.0, 0.0, -1.0, 1.0, -1.0, 0.0, -1.0, -1.0,
			1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 0.0, -1.0, 1.0, -1.0, 0.0, 1.0, 1.0, -1.0, 0.0, -1.0,
			-1.0, 1.0, 0.0, 1.0, -1.0, 1.0, 0.0, -1.0, -1.0, -1.0, 0.0, 1.0, -1.0, -1.0, 0.0, -1.0,
			1.0, 1.0, 1.0, 0.0, 1.0, 1.0, -1.0, 0.0, 1.0, -1.0, 1.0, 0.0, 1.0, -1.0, -1.0, 0.0,
			-1.0, 1.0, 1.0, 0.0, -1.0, 1.0, -1.0, 0.0, -1.0, -1.0, 1.0, 0.0, -1.0, -1.0, -1.0, 0.0
		};
		return gradient + (index % 32);
	}

	/* (tool) A lookup table to traverse the simplex around a given point in 4D. */
	static const int simplex[64][4] = {
		{ 0, 1, 2, 3 },{ 0, 1, 3, 2 },{ 0, 0, 0, 0 },{ 0, 2, 3, 1 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 1, 2, 3, 0 },
		{ 0, 2, 1, 3 },{ 0, 0, 0, 0 },{ 0, 3, 1, 2 },{ 0, 3, 2, 1 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 1, 3, 2, 0 },
		{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },
		{ 1, 2, 0, 3 },{ 0, 0, 0, 0 },{ 1, 3, 0, 2 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 2, 3, 0, 1 },{ 2, 3, 1, 0 },
		{ 1, 0, 2, 3 },{ 1, 0, 3, 2 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 2, 0, 3, 1 },{ 0, 0, 0, 0 },{ 2, 1, 3, 0 },
		{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },
		{ 2, 0, 1, 3 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 3, 0, 1, 2 },{ 3, 0, 2, 1 },{ 0, 0, 0, 0 },{ 3, 1, 2, 0 },
		{ 2, 1, 0, 3 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 0, 0, 0, 0 },{ 3, 1, 0, 2 },{ 0, 0, 0, 0 },{ 3, 2, 0, 1 },{ 3, 2, 1, 0 }
	};

	/* (tool) doubleing point modulo 289 */
	static inline double mod289(double value) {
		return (value - 289.0*std::floor(value / 289.0));
	}

	/* (tool) doubleing point hash permute */
	static inline double permute(double value) {
		return mod289(value * (1.0 + 34.0*value));
	}


	/**
	* @brief Raw Simplex noise at a 2D position
	* @return a doubleing point number in [-1.0, 1.0]
	*/
	static double simplexNoise2D(const glm::dvec2 &position)
	{
		const double half = 0.5;
		const double cst_skew = 0.5 * (1.73205081 - 1.0);		// 1.73205081 = sqrt(3.0)
		const double cst_unskew = (3.0 - 1.73205081) / 6.0;

		// Skew the input space to determine which simplex cell we're in :

		glm::dvec2 corner1(position[0], position[1]);
		double a = (position[0] + position[1]) * cst_skew;
		corner1 = corner1 + glm::dvec2(a, a);
		corner1[0] = std::floor(corner1[0]);
		corner1[1] = std::floor(corner1[1]);
		glm::dvec2 cornerOffset1 = position - corner1;
		double b = (corner1[0] + corner1[1]) * cst_unskew;
		cornerOffset1 = cornerOffset1 + glm::dvec2(b, b); // The x,y distances from the corner1 origin: Unskew first the corner1 origin back to (x,y) space
		corner1[0] = mod289(corner1[0]);
		corner1[1] = mod289(corner1[1]);

		// For the 2D case, the simplex shape is an equilateral triangle :

		glm::dvec2 corner2(0.0, 0.0);// Offsets for second (middle) corner of simplex in (i,j) coords
		if (cornerOffset1[0] > cornerOffset1[1])
			corner2[0] = 1.0; // lower triangle, XY order: (0,0)->(1,0)->(1,1)
		else corner2[1] = 1.0; // upper triangle, YX order: (0,0)->(0,1)->(1,1)
								// A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
								// a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where c = (3-sqrt(3))/6
		glm::dvec2 cornerOffset3 = cornerOffset1 + glm::dvec2(cst_unskew, cst_unskew);// Offsets for last corner in (x,y) unskewed coords
		glm::dvec2 cornerOffset2 = (cornerOffset3 - corner2);// Offsets for middle corner in (x,y) unskewed coords
		b = (cst_unskew - 1.0);
		cornerOffset3 = cornerOffset3 + glm::dvec2(b, b);

		// Evaluate contribution from the 3 hashed gradients :

		double rand1 = permute(corner1[0] + permute(corner1[1]));
		double rand2 = permute(corner1[0] + corner2[0] + permute(corner1[1] + corner2[1]));
		double rand3 = permute(corner1[0] + 1.0 + permute(corner1[1] + 1.0));
		int gi1 = static_cast<int>(rand1) % 16;
		int gi2 = static_cast<int>(rand2) % 16;
		int gi3 = static_cast<int>(rand3) % 16;

		double n1 = half - glm::dot(cornerOffset1, cornerOffset1);//noise contributions
		double n2 = half - glm::dot(cornerOffset2, cornerOffset2);
		double n3 = half - glm::dot(cornerOffset3, cornerOffset3);
		if (n1 < 0.0) n1 = 0.0;
		else {
			n1 *= n1;
			const double * gp = gradient3D(gi1);
			n1 *= n1 * glm::dot(glm::dvec2(gp[0], gp[1]), cornerOffset1);//(x,y) of gradients3D used for 2D gradient
		}
		if (n2 < 0.0) n2 = 0.0;
		else {
			n2 *= n2;
			const double * gp = gradient3D(gi2);
			n2 *= n2 * glm::dot(glm::dvec2(gp[0], gp[1]), cornerOffset2);
		}
		if (n3 < 0.0) n3 = 0.0;
		else {
			n3 *= n3;
			const double * gp = gradient3D(gi3);
			n3 *= n3 * glm::dot(glm::dvec2(gp[0], gp[1]), cornerOffset3);
		}

		//Final noise value : accumulate and scale to fit [-1,1]
		return (70.0 * (n1 + n2 + n3));
	}

	/**
	* @brief Raw Simplex noise at a 3D position
	* @return a doubleing point number in [-1.0, 1.0]
	*/
	static double simplexNoise3D(const glm::dvec3 &position)
	{
		const double cst_skew = 1.0 / 3.0f;// skew factor in 3D,
		const double cst_unskew = 1.0 / 6.0f;// unskew factor
		const double cst_contrib = 0.6f;
		const glm::dvec3 CORNER_OFFSETS[6] = {
			glm::dvec3(1.0, 0.0, 0.0),
			glm::dvec3(0.0, 1.0, 0.0),
			glm::dvec3(0.0, 0.0, 1.0),
			glm::dvec3(1.0, 1.0, 0.0),
			glm::dvec3(1.0, 0.0, 1.0),
			glm::dvec3(0.0, 1.0, 1.0) };

		// Skew/Unskew the input space to determine which simplex corner1 we're in :
		glm::dvec3 corner1(position[0], position[1], position[2]);
		double a = (position[0] + position[1] + position[2])*cst_skew;
		corner1 = corner1 + glm::dvec3(a, a, a);
		corner1[0] = floor(corner1[0]);
		corner1[1] = floor(corner1[1]);
		corner1[2] = floor(corner1[2]);// in discrete (i,j,k) coords
		glm::dvec3 cornerOffset1(position[0], position[1], position[2]);
		cornerOffset1 -= corner1;// The x,y,z offsets from the base corner
		double b = (corner1[0] + corner1[1] + corner1[2])*cst_unskew;
		cornerOffset1 = cornerOffset1 + glm::dvec3(b, b, b);
		corner1[0] = mod289(corner1[0]);
		corner1[1] = mod289(corner1[1]);
		corner1[2] = mod289(corner1[2]);

		// For the 3d case, the simplex shape is a slightly irregular tetrahedron.
		unsigned char i2, i3;
		if (cornerOffset1[0] >= cornerOffset1[1]) {
			if (cornerOffset1[1] >= cornerOffset1[2]) {// X Y Z order
				i2 = 0; // 1 0 0
				i3 = 3; // 1 1 0
			}
			else if (cornerOffset1[0] >= cornerOffset1[2]) {// X Z Y order
				i2 = 0; // 1 0 0
				i3 = 4; // 1 0 1
			}
			else {// Z X Y order
				i2 = 2; // 0 0 1
				i3 = 4; // 1 0 1
			}
		}
		else {
			if (cornerOffset1[1] < cornerOffset1[2]) {// Z Y X order
				i2 = 2; // 0 0 1
				i3 = 5; // 0 1 1
			}
			else if (cornerOffset1[0] < cornerOffset1[2]) {// Y Z X order
				i2 = 1; // 0 1 0
				i3 = 5; // 0 1 1
			}
			else {// Y X Z order
				i2 = 1; // 0 1 0
				i3 = 3; // 1 1 0
			}
		}
		glm::dvec3 corner2, corner3;
		corner2 = CORNER_OFFSETS[i2];// Offsets of the second corner of the simplex in discrete (i,j,k) coords
		corner3 = CORNER_OFFSETS[i3];// offsets of the third corner.

									 // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
									 // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
									 // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where c = cst_unskew = 1/6.
		glm::dvec3 cornerOffset4(cornerOffset1[0], cornerOffset1[1], cornerOffset1[2]);
		cornerOffset4 = cornerOffset4 + glm::dvec3(cst_unskew, cst_unskew, cst_unskew);// Offsets to the fourth corner in (x,y,z) coords
		glm::dvec3 cornerOffset2(cornerOffset4[0], cornerOffset4[1], cornerOffset4[2]);
		cornerOffset2 -= corner2;    // offsets to the second corner in (x,y,z) coords
		cornerOffset4 = cornerOffset4 + glm::dvec3(cst_unskew, cst_unskew, cst_unskew);
		glm::dvec3 cornerOffset3(cornerOffset4[0], cornerOffset4[1], cornerOffset4[2]);
		cornerOffset3 -= corner3;    // offsets to the third corner in (x,y,z) coords
		b = cst_unskew - 1.0;
		cornerOffset4 = cornerOffset4 + glm::dvec3(b, b, b);

		// Hash 4 gradients and calculate the 4 corners contributions :
		int gi1 = static_cast<int>(permute(corner1[0] + permute(corner1[1] + permute(corner1[2])))) % 16;
		int gi4 = static_cast<int>(permute(corner1[0] + 1.0 + permute(corner1[1] + 1.0 + permute(corner1[2] + 1.0)))) % 16;
		corner2 += corner1;
		corner3 += corner1;
		int gi2 = static_cast<int>(permute(corner2[0] + permute(corner2[1] + permute(corner2[2])))) % 16;
		int gi3 = static_cast<int>(permute(corner3[0] + permute(corner3[1] + permute(corner3[2])))) % 16;

		double n1, n2, n3, n4;//noise contributions
		n1 = cst_contrib - glm::dot(cornerOffset1, cornerOffset1);
		n2 = cst_contrib - glm::dot(cornerOffset2, cornerOffset2);
		n3 = cst_contrib - glm::dot(cornerOffset3, cornerOffset3);
		n4 = cst_contrib - glm::dot(cornerOffset4, cornerOffset4);
		if (n1 < 0.0) n1 = 0.0;
		else {
			n1 *= n1;
			const double * gp = gradient3D(gi1);
			n1 *= n1 * glm::dot(glm::dvec3(gp[0], gp[1], gp[2]), cornerOffset1);
		}
		if (n4 < 0.0) n4 = 0.0;
		else {
			n4 *= n4;
			const double * gp = gradient3D(gi4);
			n4 *= n4 * glm::dot(glm::dvec3(gp[0], gp[1], gp[2]), cornerOffset4);
		}
		if (n2 < 0.0) n2 = 0.0;
		else {
			n2 *= n2;
			const double * gp = gradient3D(gi2);
			n2 *= n2 * glm::dot(glm::dvec3(gp[0], gp[1], gp[2]), cornerOffset2);
		}
		if (n3 < 0.0) n3 = 0.0;
		else {
			n3 *= n3;
			const double * gp = gradient3D(gi3);
			n3 *= n3 * glm::dot(glm::dvec3(gp[0], gp[1], gp[2]), cornerOffset3);
		}

		//Final noise value : accumulate and scale to fit [-1,1]
		return (32.0*(n1 + n2 + n3 + n4));
	}



	/**
	* @brief A weighted sum of simplex noises at a given 2D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 2d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalSimplex2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;//keeping track of the largest possible amplitude,
		glm::dvec2 pos;
		for (int i = 0; i < octaves; ++i)
		{
			pos = frequency * position;
			noise += amplitude * simplexNoise2D(pos);
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return ((1.0 + noise / maxAmp)*0.5);
	}

	/**
	* @brief A weighted sum of abs(simplex noises) at a given 2D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 2d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalTurbulence2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;//keeping track of the largest possible amplitude,
		glm::dvec2 pos;
		for (int i = 0; i < octaves; ++i)
		{
			pos = frequency * position;
			noise += amplitude * std::abs(simplexNoise2D(pos));
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return noise / maxAmp;
	}

	/**
	* @brief A weighted sum of (1.0 - abs(simplex noises)) at a given 2D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 2d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalRidged2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;//keeping track of the largest possible amplitude,
		glm::dvec2 pos;
		for (int i = 0; i < octaves; ++i)
		{
			pos = position * frequency;
			noise += amplitude * (1.0 - std::abs(simplexNoise2D(pos)));
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return noise / maxAmp;
	}

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
	double fractalFilter2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position, std::pointer_to_unary_function<double, double> filter)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;//keeping track of the largest possible amplitude,
		glm::dvec2 pos;
		for (int i = 0; i < octaves; ++i)
		{
			pos = position * frequency;
			noise += amplitude * filter(simplexNoise2D(pos));
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return ((1.0 + noise / maxAmp)*0.5);
	}

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
	double fractalRidgedFilter2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position, std::pointer_to_unary_function<double, double> filter)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;//keeping track of the largest possible amplitude,
		glm::dvec2 pos;
		for (int i = 0; i < octaves; ++i)
		{
			pos = position * frequency;
			noise += amplitude * (1.0 - std::abs(filter(simplexNoise2D(pos))));
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return noise / maxAmp;
	}

	double multifractalTurbulenceFilter2D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec2 & position, std::pointer_to_unary_function<double, double> filter)
	{
		double noise = 0.0;
		double prev = 1.0;
		double n;
		double amplitude = 1.0;
		double maxAmp = 0.0;//keeping track of the largest possible amplitude,
		glm::dvec2 pos;
		for (int i = 0; i < octaves; ++i)
		{
			pos = position * frequency;
			n = std::abs(filter(simplexNoise2D(pos)));
			noise += amplitude * n * prev;
			prev = n;
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return noise / maxAmp;
	}

	/**
	* @brief A weighted sum of simplex noises at a given 3D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 3d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalSimplex3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 &position)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;//keeping track of the largest possible amplitude,
		glm::dvec3 pos(position);
		for (int i = 0; i < octaves; ++i)
		{
			pos = frequency * position;
			noise += amplitude * simplexNoise3D(pos);
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return ((1.0 + noise / maxAmp)*0.5);
	}

	/**
	* @brief A weighted sum of abs(simplex noises) at a given 3D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 3d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalTurbulence3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 &position)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;
		glm::dvec3 pos(position);
		for (int i = 0; i < octaves; ++i)
		{
			pos = frequency * position;
			noise += amplitude * std::abs(simplexNoise3D(pos));
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return noise / maxAmp;
	}

	/**
	* @brief A weighted sum of (1.0 - abs(simplex noises)) at a given 3D position.
	* @param octaves number of noise layers to accumulate
	* @param persistence ratio of the amplitude of noise layer (n+1) to the amplitude of noise layer (n) - commonly a number near 0.5
	* @param lacunarity ratio of the frequency  of noise layer (n+1) to the frequency of noise layer (n) - commonly a number near 2.0
	* @param frequency base frequency (ie, the lowest one among the noise layers)
	* @param position the 3d point where the noise is evaluated.
	* @return a number in [0, 1]
	*/
	double fractalRidged3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 &position)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;
		glm::dvec3 pos(position);
		for (int i = 0; i < octaves; ++i)
		{
			pos = frequency * position;
			noise += amplitude * (1.0 - std::abs(simplexNoise3D(pos)));
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return noise / maxAmp;
	}

	// ---------------------------------------- CELL NOISE -----------------------------------------------//

	static unsigned int hashSeed(int i, int j, int k)
	{
		// FNV hash : http://isthe.com/chongo/tech/comp/fnv/#FNV-source
		const unsigned int OFFSET_BASIS = 2166136261;
		const unsigned int FNV_PRIME = 16777619;
		const int positiveOffset = 1024 * 1024 * 16;
		i += positiveOffset;
		j += positiveOffset;
		k += positiveOffset;
		return (unsigned int)((((((OFFSET_BASIS ^ (unsigned int)i) * FNV_PRIME) ^ (unsigned int)j) * FNV_PRIME) ^ (unsigned int)k) * FNV_PRIME);
	}

	/**
	* Worley Noise - minimal euclidian distance to feature points.
	* Returns a value in [0,1]
	*/
	double cellNoise3D(const glm::dvec3 & position)
	{
		int X = (int)std::floor(position[0]);
		int Y = (int)std::floor(position[1]);
		int Z = (int)std::floor(position[2]);
		double distance = DBL_MAX;

		for (int k = -1; k <= 1; ++k)
			for (int j = -1; j <= 1; ++j)
				for (int i = -1; i <= 1; ++i)// Check 27 neighboring cells:
				{
					int cX = X + i;
					int cY = Y + j;
					int cZ = Z + k;
					cellprng.seed(hashSeed(cX, cY, cZ));
					int numFeaturePoints = 1 + cellprng() % 2;//number of feature points for this cell (at least one)
					for (int p = 0; p < numFeaturePoints; ++p)
					{
						glm::dvec3 featurePosition;
						featurePosition[0] = (double)cX + (double)(cellprng() % 65536) / 65535.0;
						featurePosition[1] = (double)cY + (double)(cellprng() % 65536) / 65535.0;
						featurePosition[2] = (double)cZ + (double)(cellprng() % 65536) / 65535.0;
						double d = glm::length(featurePosition - position);
						if (d < distance)
							distance = d;
					}
				}

		//return clamp(distance, 0.0, 1.618f) / 1.618f;
		//return clamp(distance, 0.0, 1.0) / 1.0;
		return glm::clamp(distance, 0.0, 0.75) / 0.75;
	}

	/**
	* FBM type cell noise.
	* Returns a value in [0,1]
	*/
	double fractalCellNoise3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 & position)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;

		for (int i = 0; i < octaves; ++i)
		{
			glm::dvec3 pos = frequency * position;
			noise += amplitude * cellNoise3D(pos);
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return noise / maxAmp;
	}

	/**
	* FBM type inverted cell noise. Each layer is 1.0 - cellNoise3D(...)
	* Returns a value in [0,1]
	*/
	double fractalInvertedCellNoise3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 & position)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;

		for (int i = 0; i < octaves; ++i)
		{
			glm::dvec3 pos = frequency * position;
			noise += amplitude * (1.0 - cellNoise3D(pos));
			maxAmp += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}
		return noise / maxAmp;
	}

	double fractalWarpedCellNoise3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 & position)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;
		double ws = 0.5f;

		double wx = -1.0 + 2.0f*fractalSimplex3D(1, 0.5, 2.0, frequency, position + glm::dvec3(17.7f, 17.7f, 17.7f));
		double wy = -1.0 + 2.0f*fractalSimplex3D(1, 0.5, 2.0, frequency, position + glm::dvec3(37.5f, 37.5f, 37.5f));
		double wz = -1.0 + 2.0f*fractalSimplex3D(1, 0.5, 2.0, frequency, position + glm::dvec3(-17.2f, -17.2f, -17.2f));
		glm::dvec3 warp(wx, wy, wz);

		for (int i = 0; i < octaves; ++i)
		{
			glm::dvec3 pos = frequency * position + ws*warp;
			noise += amplitude * cellNoise3D(pos);

			maxAmp += amplitude;
			amplitude *= persistence;
			ws *= persistence;
			frequency *= lacunarity;
		}
		return noise / maxAmp;
	}

	double fractalInvertedWarpedCellNoise3D(int octaves, double persistence, double lacunarity, double frequency, const glm::dvec3 & position)
	{
		double noise = 0.0;
		double amplitude = 1.0;
		double maxAmp = 0.0;
		double ws = 0.5f;

		double wx = -1.0 + 2.0f*fractalSimplex3D(1, 0.5, 2.0, frequency, position + glm::dvec3(17.7f, 17.7f, 17.7f));
		double wy = -1.0 + 2.0f*fractalSimplex3D(1, 0.5, 2.0, frequency, position + glm::dvec3(37.5f, 37.5f, 37.5f));
		double wz = -1.0 + 2.0f*fractalSimplex3D(1, 0.5, 2.0, frequency, position + glm::dvec3(-17.2f, -17.2f, -17.2f));
		glm::dvec3 warp(wx, wy, wz);

		for (int i = 0; i < octaves; ++i)
		{
			glm::dvec3 pos = frequency * position + ws*warp;
			noise += amplitude * (1.0 - cellNoise3D(pos));

			maxAmp += amplitude;
			amplitude *= persistence;
			ws *= persistence;
			frequency *= lacunarity;
		}
		return noise / maxAmp;
	}

}//end namespace tool




namespace tool
{

	bool GeometryProxy::static_init = false;
	GLuint GeometryProxy::default_vbo_quad = 0;
	GLuint GeometryProxy::default_vbo_cube = 0;
	GLuint GeometryProxy::default_vbo_sphere = 0;
	GLuint GeometryProxy::default_vao_quad = 0;
	GLuint GeometryProxy::default_vao_cube = 0;
	GLuint GeometryProxy::default_vao_sphere = 0;
	int GeometryProxy::num_sphere_triangles = 0;

	GLuint GeometryProxy::cubevbo = 0;
	GLuint GeometryProxy::cubevao = 0;

	GLuint GeometryProxy::default_vbo_cubemap[6] = { 0, 0, 0, 0, 0, 0 };
	GLuint GeometryProxy::default_vao_cubemap[6] = { 0, 0, 0, 0, 0, 0 };

	float GeometryProxy::default_vertices_cube[144] = 
	{
		// note; 4th component -1.0 is for tagging and convenient in -shader recognition.
		0.0, 0.0, 0.0, -1.0,   0.0, 1.0, 0.0, -1.0,   1.0, 1.0, 0.0, -1.0,        // Face 1 // z negative
		1.0, 1.0, 0.0, -1.0,   1.0, 0.0, 0.0, -1.0,   0.0, 0.0, 0.0, -1.0,        // Face 1

		0.0, 1.0, 0.0, -1.0,   0.0, 1.0, 1.0, -1.0,   1.0, 1.0, 0.0, -1.0,        // Face 2 // y positive
		1.0, 1.0, 0.0, -1.0,   0.0, 1.0, 1.0, -1.0,   1.0, 1.0, 1.0, -1.0,        // Face 2

		1.0, 1.0, 1.0, -1.0,   1.0, 0.0, 1.0, -1.0,   1.0, 1.0, 0.0, -1.0,        // Face 3 // x positive
		1.0, 1.0, 0.0, -1.0,   1.0, 0.0, 1.0, -1.0,   1.0, 0.0, 0.0, -1.0,        // Face 3

		1.0, 0.0, 0.0, -1.0,   1.0, 0.0, 1.0, -1.0,   0.0, 0.0, 1.0, -1.0,        // Face 4 // y negative
		0.0, 0.0, 1.0, -1.0,   0.0, 0.0, 0.0, -1.0,   1.0, 0.0, 0.0, -1.0,        // Face 4

		0.0, 0.0, 0.0, -1.0,   0.0, 1.0, 1.0, -1.0,   0.0, 1.0, 0.0, -1.0,        // Face 5 // x negative
		0.0, 0.0, 0.0, -1.0,   0.0, 0.0, 1.0, -1.0,   0.0, 1.0, 1.0, -1.0,        // Face 5

		1.0, 0.0, 1.0, -1.0,   1.0, 1.0, 1.0, -1.0,   0.0, 0.0, 1.0, -1.0,        // Face 6 // z positive
		0.0, 0.0, 1.0, -1.0,   1.0, 1.0, 1.0, -1.0,   0.0, 1.0, 1.0, -1.0,          // Face 6
	};
	float GeometryProxy::default_vertices_quad[18] = {
		-1.0, -1.0, 0.0,    1.0, 1.0, 0.0,      -1.0, 1.0, 0.0,
		-1.0, -1.0, 0.0,    1.0, -1.0, 0.0,      1.0, 1.0, 0.0
	};
	float GeometryProxy::default_texcoords_quad[12] = {
		0.0, 0.0,      1.0, 1.0,      0.0, 1.0,
		0.0, 0.0,      1.0, 0.0,      1.0, 1.0
	};

	void GeometryProxy::_init()
	{
		Q_OPENGL_FUNCS *gl = QOpenGLContext::currentContext()->versionFunctions<Q_OPENGL_FUNCS>();

		//  Buffer Objects:
		gl->glGenBuffers(1, &default_vbo_quad);
		gl->glBindBuffer(GL_ARRAY_BUFFER, default_vbo_quad);
		gl->glBufferData(GL_ARRAY_BUFFER, 18 * sizeof(float) + 12 * sizeof(float), 0, GL_STATIC_DRAW);
		gl->glBufferSubData(GL_ARRAY_BUFFER, 0, 18 * sizeof(float), default_vertices_quad);
		gl->glBufferSubData(GL_ARRAY_BUFFER, 18 * sizeof(float), 12 * sizeof(float), default_texcoords_quad);
		gl->glBindBuffer(GL_ARRAY_BUFFER, 0);

		gl->glGenBuffers(1, &default_vbo_cube);
		gl->glBindBuffer(GL_ARRAY_BUFFER, default_vbo_cube);
		gl->glBufferData(GL_ARRAY_BUFFER, 144 * sizeof(float), 0, GL_STATIC_DRAW);
		gl->glBufferSubData(GL_ARRAY_BUFFER, 0, 144 * sizeof(float), default_vertices_cube);
		gl->glBindBuffer(GL_ARRAY_BUFFER, 0);

		//  VAOS
		gl->glGenVertexArrays(1, &default_vao_quad);
		gl->glBindVertexArray(default_vao_quad);
		gl->glBindBuffer(GL_ARRAY_BUFFER, default_vbo_quad);
		gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (char*)NULL + 0);
		gl->glEnableVertexAttribArray(0);
		gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (char*)NULL + 6 * 3 * sizeof(float));
		gl->glEnableVertexAttribArray(1);
		gl->glBindVertexArray(0);

		gl->glGenVertexArrays(1, &default_vao_cube);
		gl->glBindVertexArray(default_vao_cube);
		gl->glBindBuffer(GL_ARRAY_BUFFER, default_vbo_cube);
		gl->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (char*)NULL + 0);
		gl->glEnableVertexAttribArray(0);
		gl->glBindVertexArray(0);

		// Cubemap 
		////////////
		float cm_vertices[6 * 2 * 3 * 3] = {
			// x positive:
			1.0f, 1.0f, -1.0f,    1.0f, -1.0f, -1.0f,    1.0f, -1.0f, 1.0f,   // tri 1
			1.0f, 1.0f, -1.0f,    1.0f, -1.0f, 1.0f,     1.0f, 1.0f, 1.0f,    // tri 2     
			// x negative:
			-1.0f, -1.0f, -1.0f,    -1.0f, 1.0f, -1.0f,    -1.0f, 1.0f, 1.0f,   // tri 1
			-1.0f, -1.0f, -1.0f,    -1.0f, 1.0f, 1.0f,     -1.0f, -1.0f, 1.0f,    // tri 2
			// y positive:
			-1.0f, 1.0f, -1.0f,    1.0f, 1.0f, -1.0f,    1.0f, 1.0f, 1.0f,   // tri 1
			-1.0f, 1.0f, -1.0f,    1.0f, 1.0f, 1.0f,     -1.0f, 1.0f, 1.0f,    // tri 2
			// y negative:
			1.0f, -1.0f, -1.0f,    -1.0f, -1.0f, -1.0f,    -1.0f, -1.0f, 1.0f,   // tri 1
			1.0f, -1.0f, -1.0f,    -1.0f, -1.0f, 1.0f,     1.0f, -1.0f, 1.0f,    // tri 2
			// z positive:
			-1.0f, 1.0f, 1.0f,    1.0f, 1.0f, 1.0f,    1.0f, -1.0f, 1.0f,   // tri 1
			-1.0f, 1.0f, 1.0f,    1.0f, -1.0f, 1.0f,     -1.0f, -1.0f, 1.0f,    // tri 2
			// z negative:
			-1.0f, -1.0f, -1.0f,    1.0f, -1.0f, -1.0f,    1.0f, 1.0f, -1.0f,   // tri 1
			-1.0f, -1.0f, -1.0f,    1.0f, 1.0f, -1.0f,     -1.0f, 1.0f, -1.0f     // tri 2
		};
		float cm_texcoords[6 * 2 * 3 * 2] = {
			//
			0.0f, 0.0f,    1.0f, 0.0f,    1.0f, 1.0f,
			0.0f, 0.0f,    1.0f, 1.0f,    0.0f, 1.0f,
			//
			0.0f, 0.0f,    1.0f, 0.0f,    1.0f, 1.0f,
			0.0f, 0.0f,    1.0f, 1.0f,    0.0f, 1.0f,
			//
			0.0f, 0.0f,    1.0f, 0.0f,    1.0f, 1.0f,
			0.0f, 0.0f,    1.0f, 1.0f,    0.0f, 1.0f,
			//
			0.0f, 0.0f,    1.0f, 0.0f,    1.0f, 1.0f,
			0.0f, 0.0f,    1.0f, 1.0f,    0.0f, 1.0f,
			//
			0.0f, 0.0f,    1.0f, 0.0f,    1.0f, 1.0f,
			0.0f, 0.0f,    1.0f, 1.0f,    0.0f, 1.0f,
			//
			0.0f, 0.0f,    1.0f, 0.0f,    1.0f, 1.0f,
			0.0f, 0.0f,    1.0f, 1.0f,    0.0f, 1.0f
		};

		gl->glGenBuffers(6, default_vbo_cubemap);
		gl->glGenVertexArrays(6, default_vao_cubemap);
		for (int i = 0; i < 6; ++i)
		{
			gl->glBindBuffer(GL_ARRAY_BUFFER, default_vbo_cubemap[i]);
			gl->glBufferData(GL_ARRAY_BUFFER, 2 * 3 * (3 * sizeof(float) + 2 * sizeof(float)), 0, GL_STATIC_DRAW);
			gl->glBufferSubData(GL_ARRAY_BUFFER, 0, 2 * 3 * 3 * sizeof(float), cm_vertices + i*(2 * 3 * 3));
			gl->glBufferSubData(GL_ARRAY_BUFFER, 2 * 3 * 3 * sizeof(float), 2 * 3 * 2 * sizeof(float), cm_texcoords + i*(2 * 3 * 2));
			gl->glBindBuffer(GL_ARRAY_BUFFER, 0);

			gl->glBindVertexArray(default_vao_cubemap[i]);
			gl->glBindBuffer(GL_ARRAY_BUFFER, default_vbo_cubemap[i]);
			gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (char*)NULL + 0);
			gl->glEnableVertexAttribArray(0);
			gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (char*)NULL + 2 * 3 * 3 * sizeof(float));
			gl->glEnableVertexAttribArray(1);
			gl->glBindVertexArray(0);
		}

		// --- SPHERE ---
		int numlong = 1024;
		int numlat = numlong / 2;
		float * svdata = new float[3 * (3 + 3) * numlong * numlat];
		for (int j = 0; j < numlat; ++j)
		{
			for (int i = 0; i < numlong; ++i)
			{
				float lg_0 = 2.0f * (float)PI * (float)i / (float)numlong;
				float lat_0 = (float)PI * (-0.5f + (float)j / (float)numlat);

				float lg_1 = 2.0f * (float)PI * (float)(i+1) / (float)numlong;
				float lat_1 = lat_0;

				float lg_2 = lg_1;
				float lat_2 = (float)PI * (-0.5f + (float)(j+1) / (float)numlat);

				float lg_3 = lg_0;
				float lat_3 = lat_2;

				// triangle 1
				svdata[3 * 6 * (j * numlong + i) + 0] = std::cos(lg_0) * std::cos(lat_0);
				svdata[3 * 6 * (j * numlong + i) + 1] = std::sin(lg_0) * std::cos(lat_0);
				svdata[3 * 6 * (j * numlong + i) + 2] = std::sin(lat_0);

				svdata[3 * 6 * (j * numlong + i) + 3] = std::cos(lg_1) * std::cos(lat_1);
				svdata[3 * 6 * (j * numlong + i) + 4] = std::sin(lg_1) * std::cos(lat_1);
				svdata[3 * 6 * (j * numlong + i) + 5] = std::sin(lat_1);

				svdata[3 * 6 * (j * numlong + i) + 6] = std::cos(lg_2) * std::cos(lat_2);
				svdata[3 * 6 * (j * numlong + i) + 7] = std::sin(lg_2) * std::cos(lat_2);
				svdata[3 * 6 * (j * numlong + i) + 8] = std::sin(lat_2);
				
				// triangle 2
				svdata[3 * 6 * (j * numlong + i) + 9] = std::cos(lg_0) * std::cos(lat_0);
				svdata[3 * 6 * (j * numlong + i) + 10] = std::sin(lg_0) * std::cos(lat_0);
				svdata[3 * 6 * (j * numlong + i) + 11] = std::sin(lat_0);

				svdata[3 * 6 * (j * numlong + i) + 12] = std::cos(lg_2) * std::cos(lat_2);
				svdata[3 * 6 * (j * numlong + i) + 13] = std::sin(lg_2) * std::cos(lat_2);
				svdata[3 * 6 * (j * numlong + i) + 14] = std::sin(lat_2);

				svdata[3 * 6 * (j * numlong + i) + 15] = std::cos(lg_3) * std::cos(lat_3);
				svdata[3 * 6 * (j * numlong + i) + 16] = std::sin(lg_3) * std::cos(lat_3);
				svdata[3 * 6 * (j * numlong + i) + 17] = std::sin(lat_3);
			}
		}

		num_sphere_triangles = numlong * numlat * 2;

		gl->glGenBuffers(1, &default_vbo_sphere);
		gl->glBindBuffer(GL_ARRAY_BUFFER, default_vbo_sphere);
		gl->glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * 6 * numlong * numlat, 0, GL_STATIC_DRAW);
		gl->glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 3 * 6 * numlong * numlat, svdata);
		gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
		
		gl->glGenVertexArrays(1, &default_vao_sphere);
		gl->glBindVertexArray(default_vao_sphere);
		gl->glBindBuffer(GL_ARRAY_BUFFER, default_vbo_sphere);
		gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (char*)NULL + 0);
		gl->glEnableVertexAttribArray(0);		
		gl->glBindVertexArray(0);

		delete[] svdata;

		static_init = true;		
	}

	void GeometryProxy::drawSphere()
	{
		if (!static_init)
			_init();
		Q_OPENGL_FUNCS *gl = QOpenGLContext::currentContext()->versionFunctions<Q_OPENGL_FUNCS>();
		gl->glBindVertexArray(default_vao_sphere);
		gl->glDrawArrays(GL_TRIANGLES, 0, num_sphere_triangles * 3);
		gl->glBindVertexArray(0);		
	}

	void GeometryProxy::cleanup()
	{
		if (!static_init)
			return;

		Q_OPENGL_FUNCS *gl = QOpenGLContext::currentContext()->versionFunctions<Q_OPENGL_FUNCS>();

		gl->glDeleteBuffers(1, &default_vbo_cube);
		gl->glDeleteBuffers(1, &default_vbo_quad);
		gl->glDeleteBuffers(6, default_vbo_cubemap);
		gl->glDeleteBuffers(1, &default_vbo_sphere);

		gl->glDeleteVertexArrays(1, &default_vao_cube);
		gl->glDeleteVertexArrays(1, &default_vao_quad);
		gl->glDeleteVertexArrays(6, default_vao_cubemap);
		gl->glDeleteVertexArrays(1, &default_vao_sphere);

		static_init = false;
	}
}