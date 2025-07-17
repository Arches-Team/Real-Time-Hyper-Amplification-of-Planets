#pragma once

#include "tool.h"



namespace tool
{

	class NURBSCurve
	{
	public:

		inline NURBSCurve(int degree) : m_degree(degree) {}
		
		inline ~NURBSCurve()
		{
			delete[] m_deboorcox;
			if (m_internal_nodal)
				delete[] m_nodal_sequence;
		}

		inline void setControlPoints(int num, const glm::dvec4 * control_points)
		{
			m_num_control_points = num;
			m_control_points = control_points;

			delete[] m_deboorcox;
			m_deboorcox = new glm::dvec4[m_num_control_points];
		}

		void buildUniformNodalSequence()
		{
			int n = m_num_control_points - 1;
			int k = m_degree + 1;

			if (m_internal_nodal)
				delete[] m_nodal_sequence;
			m_internal_nodal = true;
			m_nodal_sequence = new double[n + k + 1];

			for (int i = 0; i <= k - 1; ++i)
				m_nodal_sequence[i] = 0.0;
			for (int i = k; i <= n; ++i)
				m_nodal_sequence[i] = (double)(i - k + 1) / (double)(n - k + 2);
			for (int i = n + 1; i <= n + k; ++i)
				m_nodal_sequence[i] = 1.0;

			m_size_nodal_sequence = n + k + 1;
		}

		inline void setNodalSequence(int size, double * seq)
		{
			if (m_internal_nodal)
				delete[] m_nodal_sequence;
			m_internal_nodal = false;
			m_size_nodal_sequence = size;
			m_nodal_sequence = seq;
		}

		/// Evaluates the NURBS curve at a certain parametrized position
		/// @param u in [0, 1]
		glm::dvec4 getCurvePoint(double u)
		{
			if (m_nodal_sequence == nullptr)
				buildUniformNodalSequence();

			glm::dvec4 point;
			point.w = 1.0;

			double w, alpha, beta;
			int n = m_num_control_points - 1;
			int k = m_degree + 1;

			// premier point:  
			if (u == 0.0)
			{
				point.x = m_control_points[0].x;
				point.y = m_control_points[0].y;
				point.z = m_control_points[0].z;
				return point;
			}
			// dernier point:  
			if (u == 1.0)
			{
				point.x = m_control_points[n].x;
				point.y = m_control_points[n].y;
				point.z = m_control_points[n].z;
				return point;
			}

			// DeBoor-Cox:
			// initialiser le tableau:
			for (int p = 0; p < m_num_control_points; ++p)
			{
				w = m_control_points[p].w;//coordonnée homogène
				m_deboorcox[p].x = w * m_control_points[p].x;
				m_deboorcox[p].y = w * m_control_points[p].y;
				m_deboorcox[p].z = w * m_control_points[p].z;
				m_deboorcox[p].w = w;
			}

			int r = 0;
			while (m_nodal_sequence[r] < u)//calcul de r
				r++;
			r--;

			for (int j = 1; j <= m_degree; ++j)
				for (int i = r; i >= (r - k + 1 + j); --i)
				{
					alpha = (u - m_nodal_sequence[i]) / (m_nodal_sequence[i + k - j] - m_nodal_sequence[i]);
					beta = (m_nodal_sequence[i + k - j] - u) / (m_nodal_sequence[i + k - j] - m_nodal_sequence[i]);

					m_deboorcox[i].x = alpha * m_deboorcox[i].x + beta * m_deboorcox[i - 1].x;
					m_deboorcox[i].y = alpha * m_deboorcox[i].y + beta * m_deboorcox[i - 1].y;
					m_deboorcox[i].z = alpha * m_deboorcox[i].z + beta * m_deboorcox[i - 1].z;
					m_deboorcox[i].w = alpha * m_deboorcox[i].w + beta * m_deboorcox[i - 1].w;
				}

			w = m_deboorcox[r].w;
			point.x = m_deboorcox[r].x / w;
			point.y = m_deboorcox[r].y / w;
			point.z = m_deboorcox[r].z / w;

			return point;
		}


	private:

		/// The control points array
		const glm::dvec4 * m_control_points;
		/// Temporary array for DeBor-Coox algorithm
		glm::dvec4 * m_deboorcox = nullptr;
		/// The nodal sequence array
		double * m_nodal_sequence = nullptr;
		/// Degree of the NURBS
		int m_degree;
		/// Number of control points
		int m_num_control_points;
		/// Size of the nodal sequence
		int m_size_nodal_sequence;
		/// use internal nodal sequence
		bool m_internal_nodal = false;
	};	
}