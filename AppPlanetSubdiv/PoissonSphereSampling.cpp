#include "PoissonSphereSampling.h"

#include <iostream>




void PoissonSphereSampling::sample(double sphere_radius, double poisson_radius)
{
	m_sphere_radius = sphere_radius;
	m_poisson_radius = poisson_radius;

	// build acceleration structure
	const double cell_size = m_poisson_radius / std::sqrt(3.0);
	const int GRID_SIZE = 2 * ((int)std::floor(m_sphere_radius / cell_size) + 1);
	const int GRID_SIZE_SQUARE = GRID_SIZE * GRID_SIZE;
	const int GRID_SIZE_CUBE = GRID_SIZE_SQUARE * GRID_SIZE;
	int * grid = new int[GRID_SIZE_CUBE];
	for (int i = 0; i < GRID_SIZE_CUBE; ++i)
		grid[i] = -1;
	std::cout << "Fast Poisson sphere sampling using grid size: " << GRID_SIZE << std::endl;

	// start with the 6 initial octahedron vertices
	m_samples.reserve(500000);

	std::vector<int> active_list;
	active_list.reserve(500000);

	math::dvec3 v;
	math::ivec3 vi;
	int i;
	const double sq2 = std::sqrt(2.0);

	v = math::dvec3(0.0, 0.0, 1.0) * m_sphere_radius;
	vi.x = (int)std::floor(v.x / cell_size) + GRID_SIZE / 2;
	vi.y = (int)std::floor(v.y / cell_size) + GRID_SIZE / 2;
	vi.z = (int)std::floor(v.z / cell_size) + GRID_SIZE / 2;
	i = vi.z * GRID_SIZE * GRID_SIZE + vi.y * GRID_SIZE + vi.x;
	assert(i >= 0 && i < GRID_SIZE_CUBE);
	m_samples.push_back(v);
	grid[i] = 0;
	active_list.push_back(0);

	v = math::dvec3(0.0, 0.0, -1.0) * m_sphere_radius;
	vi.x = (int)std::floor(v.x / cell_size) + GRID_SIZE / 2;
	vi.y = (int)std::floor(v.y / cell_size) + GRID_SIZE / 2;
	vi.z = (int)std::floor(v.z / cell_size) + GRID_SIZE / 2;
	i = vi.z * GRID_SIZE * GRID_SIZE + vi.y * GRID_SIZE + vi.x;
	assert(i >= 0 && i < GRID_SIZE_CUBE);
	m_samples.push_back(v);
	grid[i] = 1;
	active_list.push_back(1);

	v = math::dvec3(-1.0, 0.0, 0.0) * m_sphere_radius;
	vi.x = (int)std::floor(v.x / cell_size) + GRID_SIZE / 2;
	vi.y = (int)std::floor(v.y / cell_size) + GRID_SIZE / 2;
	vi.z = (int)std::floor(v.z / cell_size) + GRID_SIZE / 2;
	i = vi.z * GRID_SIZE * GRID_SIZE + vi.y * GRID_SIZE + vi.x;
	assert(i >= 0 && i < GRID_SIZE_CUBE);
	m_samples.push_back(v);
	grid[i] = 2;
	active_list.push_back(2);

	v = math::dvec3(0.0, -1.0, 0.0) * m_sphere_radius;
	vi.x = (int)std::floor(v.x / cell_size) + GRID_SIZE / 2;
	vi.y = (int)std::floor(v.y / cell_size) + GRID_SIZE / 2;
	vi.z = (int)std::floor(v.z / cell_size) + GRID_SIZE / 2;
	i = vi.z * GRID_SIZE * GRID_SIZE + vi.y * GRID_SIZE + vi.x;
	assert(i >= 0 && i < GRID_SIZE_CUBE);
	m_samples.push_back(v);
	grid[i] = 3;
	active_list.push_back(3);

	v = math::dvec3(1.0, 0.0, 0.0) * m_sphere_radius;
	vi.x = (int)std::floor(v.x / cell_size) + GRID_SIZE / 2;
	vi.y = (int)std::floor(v.y / cell_size) + GRID_SIZE / 2;
	vi.z = (int)std::floor(v.z / cell_size) + GRID_SIZE / 2;
	i = vi.z * GRID_SIZE * GRID_SIZE + vi.y * GRID_SIZE + vi.x;
	assert(i >= 0 && i < GRID_SIZE_CUBE);
	m_samples.push_back(v);
	grid[i] = 4;
	active_list.push_back(4);

	v = math::dvec3(0.0, 1.0, 0.0) * m_sphere_radius;
	vi.x = (int)std::floor(v.x / cell_size) + GRID_SIZE / 2;
	vi.y = (int)std::floor(v.y / cell_size) + GRID_SIZE / 2;
	vi.z = (int)std::floor(v.z / cell_size) + GRID_SIZE / 2;
	i = vi.z * GRID_SIZE * GRID_SIZE + vi.y * GRID_SIZE + vi.x;
	assert(i >= 0 && i < GRID_SIZE_CUBE);
	m_samples.push_back(v);
	grid[i] = 5;
	active_list.push_back(5);

	// perform fast poisson sampling (see Bridson : https://www.cct.lsu.edu/~fharhad/ganbatte/siggraph2007/CD2/content/sketches/0250.pdf)
	std::mt19937 prng(13337);
	const int max_candidates = 32;
	while (!active_list.empty())
	{
		int active_index = prng() % active_list.size();
		const math::dvec3 p = m_samples[active_list[active_index]];		
		const math::dvec3 N = math::normalize(p);
		tool::MonoVectorFrame tangentFrame(N);
		bool reject = true;

		for (int t = 0; t < max_candidates; ++t)
		{
			math::dvec3 offset(0.0);
			do {
				offset.x = -1.0 + 2.0*(double)(prng() % 65536) / 65535.0;
				offset.y = -1.0 + 2.0*(double)(prng() % 65536) / 65535.0;
			} while (math::length(offset) < 0.001);

			offset = math::normalize(offset);
			offset *= m_poisson_radius * (1.0 + (double)(prng() % 65536) / 65535.0);

			math::dvec3 candidateSample = p + offset.x * tangentFrame.t + offset.y * tangentFrame.b;
			candidateSample = m_sphere_radius * math::normalize(candidateSample);

			math::ivec3 cell;
			cell.x = (int)std::floor(candidateSample.x / cell_size) + GRID_SIZE / 2;
			cell.y = (int)std::floor(candidateSample.y / cell_size) + GRID_SIZE / 2;
			cell.z = (int)std::floor(candidateSample.z / cell_size) + GRID_SIZE / 2;
			
			bool bad_candidate = false;
			for (int k = -1; k <= 1; ++k)
			{
				if (bad_candidate)
					break;
				for (int j = -1; j <= 1; ++j)
				{
					if (bad_candidate)
						break;
					for (int i = -1; i <= 1; ++i)
					{
						int X, Y, Z;
						X = cell.x + i;
						Y = cell.y + j;
						Z = cell.z + k;
						if (X < 0 || X >= GRID_SIZE || Y < 0 || Y >= GRID_SIZE || Z < 0 || Z >= GRID_SIZE)
							continue;
						const int cell_index = Z * GRID_SIZE_SQUARE + Y * GRID_SIZE + X;
						const int sample_index = grid[cell_index];
						if (sample_index == -1)
							continue;

						if (math::distance(candidateSample, m_samples[sample_index]) < m_poisson_radius)
						{
							bad_candidate = true;
							break;
						}
					}
				}
			}

			if (!bad_candidate)
			{
				const int new_sample_index = m_samples.size();
				grid[cell.z * GRID_SIZE_SQUARE + cell.y * GRID_SIZE + cell.x] = new_sample_index;
				m_samples.push_back(candidateSample);
				active_list.push_back(new_sample_index);
				reject = false;
				break;
			}
		}

		if (reject)
		{
			int last_index = active_list.back();
			active_list[active_index] = last_index;
			active_list.pop_back();
		}
	}

	// cleanup
	delete[] grid;
	std::cout << "Poisson sampling: " << m_samples.size() << " samples generated." << std::endl;
}
