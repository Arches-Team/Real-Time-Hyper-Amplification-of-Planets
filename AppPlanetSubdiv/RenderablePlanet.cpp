#include "RenderablePlanet.h"

#include "tool.h"

#include <cmath>
#include <iostream>
#include <list>
#include <set>
#include <unordered_map>

#include <qimage.h>

#include "SphericalDelaunay.h"




bool RenderablePlanet::init(int viewportWidth, int viewportHeight, GLuint default_fbo, std::ostream & shader_log)
{
	// --- make base mesh ---
	auto start = std::chrono::high_resolution_clock::now();
	
	makePoissonDelaunayBaseMesh();
	
	std::chrono::duration<double> basetime = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - start);
	double minutes = std::floor(basetime.count() / 60.0);
	double seconds = basetime.count() - 60.0 * minutes;
	std::cout << std::endl << "=====================================================\nPoisson sampling, spherical delaunay and mesh conversion took : " << minutes << " minutes " << seconds << " seconds." << std::endl;

	std::cout << "Cleaning up coasts + computing base river network ..." << std::endl;
	createBaseRiverNetwork();
	postprocessBaseRiverNetwork();

	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - start);
	minutes = std::floor(time_span.count() / 60.0);
	seconds = time_span.count() - 60.0 * minutes;
	std::cout << std::endl << "Total took : " << minutes << " minutes " << seconds << " seconds.\n=====================================================\n" << std::endl;

	// -- initialize opengl video memory and objects --
	m_viewwidth = viewportWidth;
	m_viewheight = viewportHeight;
	m_default_fbo = default_fbo;
	if (!initGL(shader_log))
		return false;

	return true;
}



void RenderablePlanet::release()
{
	delete[] m_river_nodes;

	glDeleteQueries(1, &m_timequery);

	//glDeleteTextures(1, &m_terrainTextureArray);
	glDeleteTextures(1, &m_mountainGrassTexture);
	glDeleteTextures(1, &m_rockTexture);
	glDeleteTextures(1, &m_desertTexture);
	glDeleteTextures(1, &m_snowTexture);

	glDeleteTextures(1, &m_noiseTexture3D);

	glDeleteBuffers(1, &m_edge_buffer);
	glDeleteBuffers(1, &m_vertex_buffer);
	glDeleteBuffers(1, &m_face_buffer);

	glDeleteBuffers(1, &m_edge_counter);
	glDeleteBuffers(1, &m_vertex_counter);
	glDeleteBuffers(1, &m_face_counter);
	glDeleteBuffers(1, &m_ibo_counter);
	glDeleteBuffers(1, &m_water_ibo_counter);

	glDeleteBuffers(1, &m_pos_vbo);
	glDeleteBuffers(1, &m_ibo);
	glDeleteBuffers(1, &m_water_vbo);
	glDeleteBuffers(1, &m_water_ibo);

	glDeleteVertexArrays(1, &m_vao);
	glDeleteVertexArrays(1, &m_water_vao);

	m_compute_edgesplit->destroy();
	delete m_compute_edgesplit;
	m_compute_edgesplit_simple->destroy();
	delete m_compute_edgesplit_simple;
	
	m_compute_ghostmarking->destroy();
	delete m_compute_ghostmarking;
	m_compute_ghostsplit->destroy();
	delete m_compute_ghostsplit;

	m_compute_facesplit_riverbranching->destroy();
	delete m_compute_facesplit_riverbranching;
	m_compute_facesplit_erosion->destroy();
	delete m_compute_facesplit_erosion;
	m_compute_facesplit_simple->destroy();
	delete m_compute_facesplit_simple;

	m_compute_profiles->destroy();
	delete m_compute_profiles;
	m_compute_postprocess1->destroy();
	delete m_compute_postprocess1;
	m_compute_postprocess2->destroy();
	delete m_compute_postprocess2;
	m_compute_water_0->destroy();
	delete m_compute_water_0;
	m_compute_water_1->destroy();
	delete m_compute_water_1;
	m_compute_waterghosts->destroy();
	delete m_compute_waterghosts;

	m_compute_ibo->destroy();
	delete m_compute_ibo;

	m_render_test->destroy();
	delete m_render_test;

	if (m_geometry_terrain_pass != nullptr)
	{
		m_geometry_terrain_pass->destroy();
		delete m_geometry_terrain_pass;
	}
	if (m_geometry_water_pass != nullptr)
	{
		m_geometry_water_pass->destroy();
		delete m_geometry_water_pass;
	}
	if (m_render_deferred_final != nullptr)
	{
		m_render_deferred_final->destroy();
		delete m_render_deferred_final;
	}

	glDeleteTextures(1, &m_water_geometry_buffer_0);
	glDeleteTextures(1, &m_water_geometry_buffer_1);
	glDeleteTextures(1, &m_geometry_buffer_0);
	glDeleteTextures(1, &m_geometry_buffer_1);
	glDeleteTextures(1, &m_geometry_buffer_2);
	glDeleteFramebuffers(1, &m_framebuffer_terrain);
	glDeleteRenderbuffers(1, &m_depth_buffer_terrain);
	glDeleteFramebuffers(1, &m_framebuffer_water);
	glDeleteRenderbuffers(1, &m_depth_buffer_water);
}

void RenderablePlanet::render(double timeSeconds, int viewwidth, int viewheight, const math::dvec3 & cameraPosition, const math::dmat4 & view, const math::dmat4 & projection)
{
	// Animate Water
	if (!m_option_debug_shading)
	{
		// Displace & animate water surface
		GLuint prog = m_compute_water_0->getProgramID();
		glUseProgram(prog);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_water_vbo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_edge_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_face_buffer);

		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_3D, m_noiseTexture3D);
		glUniform1i(glGetUniformLocation(prog, "noiseTexture"), 0);

		glUniform1ui(glGetUniformLocation(prog, "lastVertexIndex"), m_vertex_end -1);
		glUniform1d(glGetUniformLocation(prog, "renderScale"), RENDER_SCALE);
		glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
		glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);
		glUniform1d(glGetUniformLocation(prog, "timeSeconds64"), timeSeconds);
		math::dvec3 camposKm = cameraPosition * RENDER_SCALE;
		glUniform3d(glGetUniformLocation(prog, "cameraPositionKm"), camposKm.x, camposKm.y, camposKm.z);
		glUniform1ui(glGetUniformLocation(prog, "optionRiverPrimitives"), m_option_river_primitives ? 1 : 0);

		const GLuint compute_local_size = 128;
		GLuint compute_num_groups = 1 + (m_vertex_end) / compute_local_size;
		glDispatchCompute(compute_num_groups, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);// | GL_BUFFER_UPDATE_BARRIER_BIT);

		// Reposition ghost water vertices : pretty useless for some reasons
		/*prog = m_compute_waterghosts->getProgramID();
		glUseProgram(prog);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_edge_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_vertex_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_water_vbo);

		glUniform1ui(glGetUniformLocation(prog, "lastEdgeIndex"), m_edge_end - 1);		
		glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
		glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);
		
		compute_num_groups = 1 + (m_edge_end) / compute_local_size;
		glDispatchCompute(compute_num_groups, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);// | GL_BUFFER_UPDATE_BARRIER_BIT);*/
		
		// Compute water normals
		prog = m_compute_water_1->getProgramID();
		glUseProgram(prog);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_edge_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_face_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_vertex_buffer);		
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_water_vbo);

		glUniform1ui(glGetUniformLocation(prog, "lastVertexIndex"), m_vertex_end - 1);
		glUniform1d(glGetUniformLocation(prog, "renderScale"), RENDER_SCALE);
		glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);

		compute_num_groups = 1 + (m_vertex_end) / compute_local_size;
		glDispatchCompute(compute_num_groups, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
	}

	// --- construct terrain IBO and water IBO ---
	GLuint current = 0;//reset counters
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_ibo_counter);
	glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &current);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_water_ibo_counter);//!
	glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &current);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);// | GL_ATOMIC_COUNTER_BARRIER_BIT);
	
	GLuint prog = m_compute_ibo->getProgramID();
	glUseProgram(prog);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_edge_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_face_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_pos_vbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_ibo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_water_ibo);//!
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 5, m_ibo_counter);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, m_water_vbo);//!
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 7, m_water_ibo_counter);//!

	glUniform1ui(glGetUniformLocation(prog, "lastFaceIndex"), m_face_end - 1);
	glUniform3d(glGetUniformLocation(prog, "cameraPosition"), cameraPosition.x, cameraPosition.y, cameraPosition.z);
	glUniform1d(glGetUniformLocation(prog, "renderScale"), RENDER_SCALE);
	glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
	glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);
	const math::dmat4 projview = projection * view;
	glUniformMatrix4dv(glGetUniformLocation(prog, "projview"), 1, GL_FALSE, glm::value_ptr(projview));

	const GLuint compute_local_size = 128;
	GLuint compute_num_groups = 1 + (m_face_end) / compute_local_size;
	glDispatchCompute(compute_num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_ibo_counter);
	GLuint counterval;
	glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &counterval);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	m_ibo_count = counterval;

	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_water_ibo_counter);
	glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &counterval);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	m_water_ibo_count = counterval;
	
	//std::cout << m_ibo_count << " | " << m_water_ibo_count << std::endl;
	glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);

	// --- render ---
	if (m_option_debug_shading)
	{
		prog = m_render_test->getProgramID();
		glUseProgram(prog);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glEnable(GL_DEPTH_TEST);
		if (m_option_wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_vertex_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_face_buffer);

		glUniform1ui(glGetUniformLocation(prog, "showGhostVertices"), m_option_show_ghostvertices ? 1 : 0);
		glUniform1ui(glGetUniformLocation(prog, "showFlowDirection"), m_option_show_flow_direction ? 1 : 0);
		glUniform1ui(glGetUniformLocation(prog, "showProfileDistance"), m_option_generate_valley_profiles  && m_option_show_profile_distance ? 1 : 0);
		glUniform1ui(glGetUniformLocation(prog, "showDrainage"), m_option_show_drainage  && m_option_generate_drainage ? 1 : 0);
		glUniform1ui(glGetUniformLocation(prog, "showTectonicAge"), m_option_show_tectonic_age ? 1 : 0);
		glUniform1ui(glGetUniformLocation(prog, "showPlateaux"), m_option_show_plateaux_presence ? 1 : 0);
		glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);
		glUniform3f(glGetUniformLocation(prog, "sunDirection"), (float)m_sun_direction.x, (float)m_sun_direction.y, (float)m_sun_direction.z);

		glUniformMatrix4dv(glGetUniformLocation(prog, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4dv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
		glUniform1d(glGetUniformLocation(prog, "farPlane"), m_camera_farplane_km / RENDER_SCALE);
		glUniform1d(glGetUniformLocation(prog, "renderScale"), RENDER_SCALE);

		glBindVertexArray(m_vao);
		glDrawElements(GL_TRIANGLES, m_ibo_count * 3, GL_UNSIGNED_INT, (void*)NULL);
		glBindVertexArray(0);

		glUseProgram(0);
		if (m_option_wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	else
	{// -- DEFERRED SHADING for HQ RENDERING OF THE PLANET --
	// -------------------------------------------------------
		if (m_viewport_changed || viewwidth != m_viewwidth || viewheight != m_viewheight)
		{
			m_viewwidth = viewwidth;
			m_viewheight = viewheight;
			buildRenderTargets();
			m_viewport_changed = false;
		}
		else {
			m_viewwidth = viewwidth;
			m_viewheight = viewheight;
		}

		// - PASS 1 : Terrain Geometry Pass -
		glDisable(GL_MULTISAMPLE);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		GLuint prog = m_geometry_terrain_pass->getProgramID();
		glUseProgram(prog);
		glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer_terrain);
		glViewport(0, 0, m_viewwidth, m_viewheight);
		constexpr float BLACK[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		constexpr float ONE = 1.0f;
		glClearBufferfv(GL_COLOR, 0, BLACK);
		glClearBufferfv(GL_COLOR, 1, BLACK);
		glClearBufferfv(GL_COLOR, 2, BLACK);
		glClearBufferfv(GL_DEPTH, 0, &ONE);

		glUniformMatrix4dv(glGetUniformLocation(prog, "projview"), 1, GL_FALSE, glm::value_ptr(projview));
		glUniform3d(glGetUniformLocation(prog, "cameraPosition"), cameraPosition.x, cameraPosition.y, cameraPosition.z);
		glUniform1d(glGetUniformLocation(prog, "farPlane"), m_camera_farplane_km / RENDER_SCALE);
		glUniform1d(glGetUniformLocation(prog, "renderScale"), RENDER_SCALE);
		glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
		glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);

		glBindVertexArray(m_vao);
		glDrawElements(GL_TRIANGLES, m_ibo_count * 3, GL_UNSIGNED_INT, (void*)NULL);
		glBindVertexArray(0);

		// - PASS 2 : Water Geometry Pass -
		prog = m_geometry_water_pass->getProgramID();
		glUseProgram(prog);
		glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer_water);
		glViewport(0, 0, m_viewwidth, m_viewheight);
		glClearBufferfv(GL_COLOR, 0, BLACK);
		glClearBufferfv(GL_COLOR, 1, BLACK);
		glClearBufferfv(GL_DEPTH, 0, &ONE);

		glUniformMatrix4dv(glGetUniformLocation(prog, "projview"), 1, GL_FALSE, glm::value_ptr(projview));
		glUniform3d(glGetUniformLocation(prog, "cameraPosition"), cameraPosition.x, cameraPosition.y, cameraPosition.z);
		glUniform1d(glGetUniformLocation(prog, "farPlane"), m_camera_farplane_km / RENDER_SCALE);
		glUniform1d(glGetUniformLocation(prog, "renderScale"), RENDER_SCALE);
		glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
		glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);

		glBindVertexArray(m_water_vao);
		glDrawElements(GL_TRIANGLES, m_water_ibo_count * 3, GL_UNSIGNED_INT, (void*)NULL);
		glBindVertexArray(0);

		glUseProgram(0);
		glBindFramebuffer(GL_FRAMEBUFFER, m_default_fbo);

		// - PASS 3 : Shading -	
		glEnable(GL_DEPTH_TEST);
		glCullFace(GL_FRONT);
		glViewport(0, 0, m_viewwidth, m_viewheight);

		prog = m_render_deferred_final->getProgramID();
		glUseProgram(prog);

		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D, m_geometry_buffer_0);
		glUniform1i(glGetUniformLocation(prog, "terrainGeometryBuffer0"), 0);

		glActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_2D, m_geometry_buffer_1);
		glUniform1i(glGetUniformLocation(prog, "terrainGeometryBuffer1"), 1);

		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, m_geometry_buffer_2);
		glUniform1i(glGetUniformLocation(prog, "terrainGeometryBuffer2"), 2);

		glActiveTexture(GL_TEXTURE0 + 3);
		glBindTexture(GL_TEXTURE_2D, m_water_geometry_buffer_0);
		glUniform1i(glGetUniformLocation(prog, "waterGeometryBuffer0"), 3);

		glActiveTexture(GL_TEXTURE0 + 4);
		glBindTexture(GL_TEXTURE_2D, m_water_geometry_buffer_1);
		glUniform1i(glGetUniformLocation(prog, "waterGeometryBuffer1"), 4);

		glActiveTexture(GL_TEXTURE0 + 5);
		glBindTexture(GL_TEXTURE_3D, m_noiseTexture3D);
		glUniform1i(glGetUniformLocation(prog, "noiseTexture"), 5);

		/*glActiveTexture(GL_TEXTURE0 + 6);
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);
		glUniform1i(glGetUniformLocation(prog, "terrainTexture"), 6);*/

		glActiveTexture(GL_TEXTURE0 + 6);
		glBindTexture(GL_TEXTURE_2D, m_rockTexture);
		glUniform1i(glGetUniformLocation(prog, "rockTexture"), 6);

		glActiveTexture(GL_TEXTURE0 + 7);
		glBindTexture(GL_TEXTURE_2D, m_mountainGrassTexture);
		glUniform1i(glGetUniformLocation(prog, "grassTexture"), 7);

		glActiveTexture(GL_TEXTURE0 + 8);
		glBindTexture(GL_TEXTURE_2D, m_desertTexture);
		glUniform1i(glGetUniformLocation(prog, "desertTexture"), 8);

		glActiveTexture(GL_TEXTURE0 + 9);
		glBindTexture(GL_TEXTURE_2D, m_snowTexture);
		glUniform1i(glGetUniformLocation(prog, "snowTexture"), 9);

		math::dmat4 cubeModel = math::translate(math::dmat4(1.0), cameraPosition - math::dvec3(0.5));
		glUniformMatrix4dv(glGetUniformLocation(prog, "model"), 1, GL_FALSE, glm::value_ptr(cubeModel));
		glUniformMatrix4dv(glGetUniformLocation(prog, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4dv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
		const math::dmat4 inverseVP = math::inverse(projview);
		glUniformMatrix4dv(glGetUniformLocation(prog, "inverseViewProj"), 1, GL_FALSE, glm::value_ptr(inverseVP));
		glUniform2d(glGetUniformLocation(prog, "screenSizePixels"), (double)m_viewwidth, (double)m_viewheight);		
		glUniform1d(glGetUniformLocation(prog, "renderScale"), RENDER_SCALE);

		glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
		glUniform1f(glGetUniformLocation(prog, "seaLevelKm"), (float)m_planet->seaLevelKm);
		glUniform1f(glGetUniformLocation(prog, "atmosphereDepthKm"), 90.0f);
		glUniform3d(glGetUniformLocation(prog, "cameraPosition"), cameraPosition.x, cameraPosition.y, cameraPosition.z);
		glUniform3f(glGetUniformLocation(prog, "sunDirection"), (float)m_sun_direction.x, (float)m_sun_direction.y, (float)m_sun_direction.z);

		glBindVertexArray(tool::GeometryProxy::get_default_cube_vao());
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);

		glUseProgram(0);
	}
}

void RenderablePlanet::tessellate(const math::dvec3 & cameraPositionKm, double fov, double nearplaneKm, double farplaneKm, int viewwidth, int viewheight)
{
	m_camera_position_km = cameraPositionKm;
	m_camera_nearplane_km = nearplaneKm;
	m_camera_farplane_km = farplaneKm;
	m_camera_fov = fov;
	m_viewwidth = viewwidth;
	m_viewheight = viewheight;

	m_edge_start = 0;
	m_edge_end = m_base_edges.size();

	m_vertex_start = 0;
	m_vertex_end = m_base_vertices.size();

	m_face_start = 0;
	m_face_end = m_base_triangles.size();

#ifdef SUBDIVISION_TIMER_QUERIES
	glBeginQuery(GL_TIME_ELAPSED, m_timequery);	
	double worse = 0.0;
#endif
	
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_edge_counter);
	GLuint current = m_edge_end;//reset counter
	glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &current);

	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_vertex_counter);
	current = m_vertex_end;//reset counter
	glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &current);

	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_face_counter);
	current = m_face_end;//reset counter
	glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &current);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	
	// TODO
	// make a gl mapping of the buffers (ranged version) for better performance (instead of relying on glBufferSubData)
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_edge_buffer);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(EdgeGPU) * m_base_edges.size(), (const void*)m_base_edges.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_vertex_buffer);	
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(VertexGPU) * m_base_vertices.size(), (const void*)m_base_vertices.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_face_buffer);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(TriangleGPU) * m_base_triangles.size(), (const void*)m_base_triangles.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pos_vbo);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(VertexAttributesGPU) * m_base_vertices.size(), (const void*)m_base_vattrib.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

#ifdef SUBDIVISION_TIMER_QUERIES
	glEndQuery(GL_TIME_ELAPSED);
	uint64_t t = 0;
	glGetQueryObjectui64v(m_timequery, GL_QUERY_RESULT, &t);
	double millis = (double)t / (1000.0 * 1000.0);//nano to millisecs
	m_reset_time += millis;
	m_modeling_time_avg += millis;
	worse += millis;
	glBeginQuery(GL_TIME_ELAPSED, m_timequery);
#endif

	int lod = 0;	
	m_edges_run_time = 0.0;
	m_ghost_run_time = 0.0;
	m_faces_run_time = 0.0;
	while ( doTessellationPass(lod) && lod < MAX_TESSELLATION_LOD )
	{
		lod++;		
	}

#ifdef SUBDIVISION_TIMER_QUERIES
	double ilod = 1.0/ (double)lod;
	m_edges_time += m_edges_run_time * ilod;
	m_ghost_time += m_ghost_run_time * ilod;
	m_faces_time += m_faces_run_time * ilod;	
	double subdiv = m_edges_run_time + m_ghost_run_time + m_faces_run_time;
	m_subdiv_time += subdiv;
	m_modeling_time_avg += subdiv;
	worse += subdiv;
#endif

	// -- post process tessellation (profiles, normals, etc.) --	

	//river profiles, ravins profiles, and water vertices creation:
	GLuint prog = m_compute_profiles->getProgramID();
	glUseProgram(prog);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_edge_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_face_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_vertex_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_pos_vbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_water_vbo);

	glActiveTexture(GL_TEXTURE0 + 0);
	glBindTexture(GL_TEXTURE_2D, m_profiles_texture);
	glUniform1i(glGetUniformLocation(prog, "riverbankProfilesTexture"), 0);

	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_3D, m_noiseTexture3D);
	glUniform1i(glGetUniformLocation(prog, "noiseTexture"), 1);

	glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
	glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);
	glUniform1ui(glGetUniformLocation(prog, "lastVertexIndex"), m_vertex_end - 1);
	glUniform1ui(glGetUniformLocation(prog, "optionBlendProfileAndTerrain"), m_option_blend_profiles_with_terrain ? 1 : 0);
	glUniform1ui(glGetUniformLocation(prog, "optionGenerateDrainage"), m_option_generate_drainage ? 1 : 0);
	glUniform1ui(glGetUniformLocation(prog, "optionGenerateRiverProfiles"), m_option_generate_valley_profiles ? 1 : 0);
	glUniform1ui(glGetUniformLocation(prog, "optionRiverPrimitives"), m_option_river_primitives ? 1 : 0);

	const GLuint compute_local_size = 128;
	GLuint compute_num_groups = 1 + (m_vertex_end) / compute_local_size;
	glDispatchCompute(compute_num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

#ifdef SUBDIVISION_TIMER_QUERIES
	glEndQuery(GL_TIME_ELAPSED);
	t = 0;
	glGetQueryObjectui64v(m_timequery, GL_QUERY_RESULT, &t);
	millis = (double)t / (1000.0 * 1000.0);//nano to millisecs
	m_profiles_time += millis;
	m_modeling_time_avg += millis;
	worse += millis;
	glBeginQuery(GL_TIME_ELAPSED, m_timequery);
#endif

	//reposition ghost vertices (for a crackfree mesh):
	prog = m_compute_postprocess1->getProgramID();
	glUseProgram(prog);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_edge_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_vertex_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_pos_vbo);

	glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
	glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);
	glUniform1ui(glGetUniformLocation(prog, "lastEdgeIndex"), m_edge_end - 1);
	
	compute_num_groups = 1 + (m_edge_end) / compute_local_size;
	glDispatchCompute(compute_num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);// | GL_BUFFER_UPDATE_BARRIER_BIT);	

	//normals:
	prog = m_compute_postprocess2->getProgramID();
	glUseProgram(prog);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_edge_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_face_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_vertex_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_pos_vbo);
	
	glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
	glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);
	glUniform1ui(glGetUniformLocation(prog, "lastVertexIndex"), m_vertex_end - 1);
	glUniform1d(glGetUniformLocation(prog, "renderScale"), RENDER_SCALE);
	
	compute_num_groups = 1 + (m_vertex_end) / compute_local_size;
	glDispatchCompute(compute_num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);// | GL_BUFFER_UPDATE_BARRIER_BIT);
	glUseProgram(0);

#ifdef SUBDIVISION_TIMER_QUERIES
	glEndQuery(GL_TIME_ELAPSED);
	t = 0;
	glGetQueryObjectui64v(m_timequery, GL_QUERY_RESULT, &t);
	millis = (double)t / (1000.0 * 1000.0);//nano to millisecs
	m_postprocess_time += millis;
	m_modeling_time_avg += millis;
	worse += millis;

	if (worse > m_modeling_time_worse)
		m_modeling_time_worse = worse;
	m_modeling_runs++;
	
	if (m_modeling_runs % 4 == 0)
	{
		double iruns = 1.0 / (double)m_modeling_runs;
		std::cout << "----- TESSELLATION Benchmark (LOD " << lod << ") ----- \n   Total = " << worse << " ms on this run (total avg = " << m_modeling_time_avg * iruns << ", worse = " << m_modeling_time_worse << "\n";
		std::cout << "   Edges = " << m_edges_run_time * ilod << " ms avg on this run (total avg " << m_edges_time * iruns << ")\n";
		std::cout << "   Ghost = " << m_ghost_run_time * ilod << " ms avg on this run (total avg " << m_ghost_time * iruns << ")\n";
		std::cout << "   Faces = " << m_faces_run_time * ilod << " ms avg on this run (total avg " << m_faces_time * iruns << ")\n";
		std::cout << "   Profiles = " << m_profiles_time * iruns << " ms total avg\n";
		std::cout << "   Postprocess = " << m_postprocess_time * iruns << " ms total avg\n" << std::endl;
	}
#endif

	std::cout << "TESSELLATION: " << lod //+ BASE_MESH_SUBDIVISION_LEVELS
		<< " subdivision levels done (total vertices " << m_vertex_end
		<< " - total triangles processed " << m_face_end
		<< " - rendered triangles in previous frame (terrain " << m_ibo_count << ", water " << m_water_ibo_count << "))" << std::endl;	
}

bool RenderablePlanet::doTessellationPass(int lod)
{
	const double cameraAltitudeKm = math::length(m_camera_position_km) - (m_planet->radiusKm + m_planet->seaLevelKm);

	//  - Edge split -
	GLuint prog = m_compute_edgesplit->getProgramID();
	if (lod > 9)
		prog = m_compute_edgesplit_simple->getProgramID();
	glUseProgram(prog);
	
	bindBuffers();
	glUniform1ui(glGetUniformLocation(prog, "baseEdgeIndex"), m_edge_start);
	glUniform1ui(glGetUniformLocation(prog, "lastEdgeIndex"), m_edge_end -1);
	glUniform3d(glGetUniformLocation(prog, "cameraPositionKm"), m_camera_position_km.x, m_camera_position_km.y, m_camera_position_km.z);
	glUniform1d(glGetUniformLocation(prog, "cameraNearPlaneKm"), m_camera_nearplane_km);
	glUniform1f(glGetUniformLocation(prog, "cameraVerticalFOV"), (float)m_camera_fov);
	glUniform1d(glGetUniformLocation(prog, "screenHeightPixels"), (double)m_viewheight);
	const double targetEdgeSize = math::mix(TARGET_EDGE_SIZE_PIXELS - 2.0, TARGET_EDGE_SIZE_PIXELS, math::clamp(cameraAltitudeKm - 200.0, 0.0, 400.0) / 200.0); //
	glUniform1d(glGetUniformLocation(prog, "edgeLengthPixels_criterion"), targetEdgeSize);
	glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
	glUniform1f(glGetUniformLocation(prog, "seaLevelKm"), (float)m_planet->seaLevelKm);
	glUniform1ui(glGetUniformLocation(prog, "optionGenerateDrainage"), m_option_generate_drainage ? 1 : 0);
	glUniform1ui(glGetUniformLocation(prog, "optionGenerateLakes"), m_option_generate_lakes ? 1 : 0);
	glUniform1ui(glGetUniformLocation(prog, "optionRiverPrimitives"), m_option_river_primitives ? 1 : 0);

	const GLuint compute_local_size = 128;
	GLuint compute_num_groups = 1 + (m_edge_end - m_edge_start) / compute_local_size;
	glDispatchCompute(compute_num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_edge_counter);
	GLuint counterval;
	glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &counterval);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	if (counterval == m_edge_end)
	{
		return false;// termination case where no more subdivision of edges needs be done.
	}

#ifdef SUBDIVISION_TIMER_QUERIES
	glEndQuery(GL_TIME_ELAPSED);
	uint64_t t = 0;
	glGetQueryObjectui64v(m_timequery, GL_QUERY_RESULT, &t);
	double millis = (double)t / (1000.0 * 1000.0);//nano to millisecs	
	m_edges_run_time += millis;
	glBeginQuery(GL_TIME_ELAPSED, m_timequery);
#endif

	//  - Ghost vertices marking -
	prog = m_compute_ghostmarking->getProgramID();
	glUseProgram(prog);	

	bindBuffers();
	glUniform1ui(glGetUniformLocation(prog, "baseFaceIndex"), m_face_start);
	glUniform1ui(glGetUniformLocation(prog, "lastFaceIndex"), m_face_end - 1);

	compute_num_groups = 1 + (m_face_end - m_face_start) / compute_local_size;
	glDispatchCompute(compute_num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	//  - Ghost split -
	prog = m_compute_ghostsplit->getProgramID();
	glUseProgram(prog);

	bindBuffers();
	glUniform1ui(glGetUniformLocation(prog, "baseEdgeIndex"), m_edge_start);
	glUniform1ui(glGetUniformLocation(prog, "lastEdgeIndex"), m_edge_end - 1);
	glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
	glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);
	glUniform1ui(glGetUniformLocation(prog, "optionRiverPrimitives"), m_option_river_primitives ? 1 : 0);

	compute_num_groups = 1 + (m_edge_end - m_edge_start) / compute_local_size;
	glDispatchCompute(compute_num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
	
#ifdef SUBDIVISION_TIMER_QUERIES
	glEndQuery(GL_TIME_ELAPSED);
	t = 0;
	glGetQueryObjectui64v(m_timequery, GL_QUERY_RESULT, &t);
	millis = (double)t / (1000.0 * 1000.0);//nano to millisecs	
	m_ghost_run_time += millis;
	glBeginQuery(GL_TIME_ELAPSED, m_timequery);
#endif

	//  - Face split -
	prog = m_compute_facesplit_riverbranching->getProgramID();
	if (lod > 3 && lod < 8)
		prog = m_compute_facesplit_erosion->getProgramID();
	else if (lod >= 8)
		prog = m_compute_facesplit_simple->getProgramID();
	glUseProgram(prog);

	bindBuffers();
	glUniform1ui(glGetUniformLocation(prog, "baseFaceIndex"), m_face_start);
	glUniform1ui(glGetUniformLocation(prog, "lastFaceIndex"), m_face_end - 1);
	glUniform1d(glGetUniformLocation(prog, "planetRadiusKm"), m_planet->radiusKm);
	glUniform1d(glGetUniformLocation(prog, "seaLevelKm"), m_planet->seaLevelKm);
	glUniform1d(glGetUniformLocation(prog, "renderScale"), RENDER_SCALE);
	glUniform1ui(glGetUniformLocation(prog, "optionGenerateDrainage"), m_option_generate_drainage ? 1 : 0);
	glUniform1ui(glGetUniformLocation(prog, "optionRiverPrimitives"), m_option_river_primitives ? 1 : 0);

	compute_num_groups = 1 + (m_face_end - m_face_start) / compute_local_size;
	glDispatchCompute(compute_num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

	//  - update counters -
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_edge_counter);
	counterval;
	glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &counterval);
	m_edge_start = m_edge_end;
	m_edge_end = counterval;

	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_vertex_counter);
	glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &counterval);
	m_vertex_start = m_vertex_end;
	m_vertex_end = counterval;

	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_face_counter);
	glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 1, &counterval);
	m_face_start = m_face_end;
	m_face_end = counterval;

	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

#ifdef SUBDIVISION_TIMER_QUERIES
	glEndQuery(GL_TIME_ELAPSED);
	t = 0;
	glGetQueryObjectui64v(m_timequery, GL_QUERY_RESULT, &t);
	millis = (double)t / (1000.0 * 1000.0);//nano to millisecs	
	m_faces_run_time += millis;
	glBeginQuery(GL_TIME_ELAPSED, m_timequery);
#endif

	return true;
}


void RenderablePlanet::makePoissonDelaunayBaseMesh()
{
	const double MINIMUM_EDGE_LENGTH_KM = 40.0;

	// Make a Poisson disk sampling of the surface of the planet
	// Build a spherical Delaunay triangulation (SDT)
	SphericalDelaunay sdt(m_planet->radiusKm, MINIMUM_EDGE_LENGTH_KM);
	//sdt.persistToDisk(std::string("../assets/delaunay/test.sdt"));

	const int vrange = sdt.getVerticesRange();
	const SphericalVertex * vs = sdt.getVertices();
	const int trange = sdt.getTrianglesRange();
	const SphericalTriangle * ts = sdt.getTriangles();

	// Edges construction:
	std::unordered_map<uint64_t, SphericalEdge> edges;// constant-time-access map
	for (int i = 0; i < trange; ++i)
	{
		const SphericalTriangle & T = ts[i];
		if (TRIANGLE_DELETED(T))
			continue;

		int v0 = T.vertex[0];
		int v1 = T.vertex[1];
		int v2 = T.vertex[2];

		uint64_t akey01 = sdt.hashEdge(v0, v1);
		uint64_t akey02 = sdt.hashEdge(v0, v2);
		uint64_t akey12 = sdt.hashEdge(v1, v2);

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

	m_base_edges.reserve(edges.size());
	m_base_vertices.reserve(vrange);
	m_base_triangles.reserve(3*vrange);
	m_base_vattrib.reserve(vrange);

	// --- Assign Edges ---
	std::unordered_map<uint64_t, int> edgeLUT;
	for (auto it = edges.cbegin(); it != edges.cend(); ++it)
	{
		const SphericalEdge edge = it->second;
		EdgeGPU E;
		E.child0 = -1;
		E.child1 = -1;
		E.vm = -1;
		E.f0 = edge.triangle[0];
		E.f1 = edge.triangle[1];
		E.status = (8u << 16);
		E.v0 = edge.vertex[0];
		E.v1 = edge.vertex[1];
		E.type = TYPE_NONE;

		m_base_edges.push_back(E);

		uint64_t key = sdt.hashEdge(E.v0, E.v1);
		edgeLUT[key] = m_base_edges.size() - 1;
	}

	// --- Assign Vertices and Vertex Attributes ---
	std::set<int> incident_triangles;
	for (int i = 0; i < vrange; ++i)
	{
		const SphericalVertex & vertex = vs[i];
		if (VERTEX_DELETED(vertex))
			continue;

		math::dvec3 p = math::normalize(vertex.coordinates);

		const PlanetData::Data data = m_planet->getInterpolatedModelData(p);
		const float plateaux = m_planet->getPlateauxDistribution(p);
		const float desert = m_planet->getDesertDistribution(p);
		const float hills = m_planet->getHillsDistribution(p);
		double r = (double)(std::rand() % 65536) / 65535.0;
		double elevation = m_planet->seaLevelKm + (data.elevation - m_planet->seaLevelKm) * (0.7 + 0.3*r);// math::mix(0.4 + 0.6*r, 0.88 - 0.2*r, (double)plateaux); // RANDOMIZE (OR NOT)
		if (data.elevation <= m_planet->seaLevelKm)
			elevation = data.elevation;
		p *= m_planet->radiusKm + elevation;
		const float tectonic_age = (float)(data.age);

		m_base_vattrib.push_back(
			{
			math::dvec4(p, elevation)
			, math::dvec4(m_planet->seaLevelKm /* nearest river altitude : ad hoc value*/
				, MINIMUM_EDGE_LENGTH_KM /* distance to river: ad hoc value */
				, data.elevation /* max crust elevation */
				, m_planet->seaLevelKm) /* water altitude : ad hoc value*/
			, math::vec4(0.0f, 0.0f, 0.0f, 0.0f) // water flow : ad hoc value
			, math::vec4((float)(m_planet->seaLevelKm) /* nearest ravin altitude : ad hoc value*/
				, MINIMUM_EDGE_LENGTH_KM /* distance to ravin : ad hoc value */
				, hills, 0.0f)
			, math::vec4(tectonic_age, plateaux, desert, 0.0f)
			, math::vec4(0.0)
			}
		);

		//---
		VertexGPU V;
		V.branch_count = 0;
		V.seed = std::rand();
		if (V.seed == 0)
			V.seed = 1337;
		V.status = 0;
		if (elevation > m_planet->seaLevelKm)
			V.type = TYPE_CONTINENT;
		else V.type = TYPE_SEA;

		incident_triangles.clear();
		vertex.getIncidentTriangles(i, ts, incident_triangles);
		for (int k = 0; k < 4; ++k)
		{
			V.faces_0[k] = -1;
			V.faces_1[k] = -1;
		}
		int * f = V.faces_0;
		int k = 0;
		for (int t : incident_triangles)
		{
			f[k] = t;			
			k++;
			if (k == 4)
			{
				f = V.faces_1;
				k = 0;
			}
		}

		m_base_vertices.push_back(V);
	}


	// --- Assign Triangles ---
	for (int i = 0; i < trange; ++i)
	{
		const SphericalTriangle & T = ts[i];
		if (TRIANGLE_DELETED(T))
			continue;

		uint64_t key0 = sdt.hashEdge(T.vertex[0], T.vertex[1]);
		uint64_t key1 = sdt.hashEdge(T.vertex[1], T.vertex[2]);
		uint64_t key2 = sdt.hashEdge(T.vertex[2], T.vertex[0]);
		
		TriangleGPU t;
		t.e0 = edgeLUT[key0];
		t.e1 = edgeLUT[key1];
		t.e2 = edgeLUT[key2];

		t.normal_x = 0.0f;
		t.normal_y = 0.0f;
		t.normal_z = 0.0f;

		t.status = (8u << 8);
		t.type = TYPE_NONE;

		m_base_triangles.push_back(t);
	}

	// --
	double avg_edge_len = sdt.getAverageTriangleEdgeLength();
	std::cout << "BASE MESH : " << avg_edge_len << " average edge (km), " << m_base_triangles.size() << " triangles for last and only base LOD." << std::endl;
}

/*
/// [unused]
void RenderablePlanet::getSurroundingVertices(int edge_index, const EdgeGPU & edge, int & va, int & vb) const 
{
	const TriangleGPU & F = m_base_triangles[edge.f0];
	if (edge_index == F.e0)
	{
		const EdgeGPU & E0 = m_base_edges[F.e2];
		const EdgeGPU & E1 = m_base_edges[F.e1];
		if (E0.v0 == E1.v0 || E0.v0 == E1.v1)
			va = E0.v0;
		else va = E0.v1;
	}
	else if (edge_index == F.e1)
	{
		const EdgeGPU & E0 = m_base_edges[F.e0];
		const EdgeGPU & E1 = m_base_edges[F.e2];
		if (E0.v0 == E1.v0 || E0.v0 == E1.v1)
			va = E0.v0;
		else va = E0.v1;
	}
	else {
		const EdgeGPU & E0 = m_base_edges[F.e1];
		const EdgeGPU & E1 = m_base_edges[F.e0];
		if (E0.v0 == E1.v0 || E0.v0 == E1.v1)
			va = E0.v0;
		else va = E0.v1;
	}

	const TriangleGPU & F2 = m_base_triangles[edge.f1];
	if (edge_index == F2.e0)
	{
		const EdgeGPU & E0 = m_base_edges[F2.e2];
		const EdgeGPU & E1 = m_base_edges[F2.e1];
		if (E0.v0 == E1.v0 || E0.v0 == E1.v1)
			vb = E0.v0;
		else vb = E0.v1;
	}
	else if (edge_index == F2.e1)
	{
		const EdgeGPU & E0 = m_base_edges[F2.e0];
		const EdgeGPU & E1 = m_base_edges[F2.e2];
		if (E0.v0 == E1.v0 || E0.v0 == E1.v1)
			vb = E0.v0;
		else vb = E0.v1;
	}
	else {
		const EdgeGPU & E0 = m_base_edges[F2.e1];
		const EdgeGPU & E1 = m_base_edges[F2.e0];
		if (E0.v0 == E1.v0 || E0.v0 == E1.v1)
			vb = E0.v0;
		else vb = E0.v1;
	}
}*/
/*
bool RenderablePlanet::triangleContainsPoint(const math::dvec3 & v0, const math::dvec3 & v1, const math::dvec3 & v2, const math::dvec3 & p) const
{
	// compute the three normals (assuming the planet/sphere is centered at the origin)
	const math::dvec3 O0 = math::normalize(v0);
	const math::dvec3 O1 = math::normalize(v1);
	const math::dvec3 O2 = math::normalize(v2);
	const math::dvec3 n0 = math::cross(O1, O2);
	const math::dvec3 n1 = math::cross(O2, O0);
	const math::dvec3 n2 = math::cross(O0, O1);
	// Test:
	//bool hemisphereTest = dot(normalize(p - sphere_center), O0) > 0.0; no need for this test in all use cases of this function (optimization)
	bool test0 = math::dot(normalize(p - v1), n0) > 0.0;
	bool test1 = math::dot(normalize(p - v2), n1) > 0.0;
	bool test2 = math::dot(normalize(p - v0), n2) > 0.0;
	//return hemisphereTest && test0 && test1 && test2;	
	return test0 && test1 && test2;
}
*/
/*
bool RenderablePlanet::checkDisplacementIntegrity(const EdgeGPU & edge, const math::dvec3 & p) const
{
	const TriangleGPU & F = m_base_triangles[edge.f0];
	const EdgeGPU & e0 = m_base_edges[F.e0];
	const EdgeGPU & e1 = m_base_edges[F.e1];
	const EdgeGPU & e2 = m_base_edges[F.e2];
	bool flip0 = false;
	bool flip1 = false;
	bool flip2 = false;//determine winding of edges (as they can appear in any order)
	if (e0.v0 == e1.v0 || e0.v0 == e1.v1)
		flip0 = true;
	if (e1.v0 == e2.v0 || e1.v0 == e2.v1)
		flip1 = true;
	if (e2.v0 == e0.v0 || e2.v0 == e0.v1)
		flip2 = true;
	int v0 = (flip0 ? e0.v1 : e0.v0);
	int v1 = (flip1 ? e1.v1 : e1.v0);
	int v2 = (flip2 ? e2.v1 : e2.v0);

	// test 1st triangle:
	if (triangleContainsPoint(m_base_vattrib[v0].position, m_base_vattrib[v1].position, m_base_vattrib[v2].position, p))
		return true;

	// test 2nd triangle:
	const TriangleGPU & F1 = m_base_triangles[edge.f1];
	const EdgeGPU & e10 = m_base_edges[F.e0];
	const EdgeGPU & e11 = m_base_edges[F.e1];
	const EdgeGPU & e12 = m_base_edges[F.e2];
	flip0 = false;
	flip1 = false;
	flip2 = false;//determine winding of edges (as they can appear in any order)
	if (e10.v0 == e11.v0 || e10.v0 == e11.v1)
		flip0 = true;
	if (e11.v0 == e12.v0 || e11.v0 == e12.v1)
		flip1 = true;
	if (e12.v0 == e10.v0 || e12.v0 == e10.v1)
		flip2 = true;
	v0 = (flip0 ? e10.v1 : e10.v0);
	v1 = (flip1 ? e11.v1 : e11.v0);
	v2 = (flip2 ? e12.v1 : e12.v0);

	if (triangleContainsPoint(m_base_vattrib[v0].position, m_base_vattrib[v1].position, m_base_vattrib[v2].position, p))
		return true;
	return false;
}
*/
/*
void RenderablePlanet::splitBaseEdge(int edge_index, EdgeGPU & edge)
{
	unsigned int level = edge.status >> 16;
	volatile unsigned int ss = edge.status >> 8;
	edge.status = (ss << 8) | 2;

	// create middle vertex:
	int v0 = edge.v0;
	int v1 = edge.v1;
	unsigned int nseed = m_base_vertices[v0].seed + m_base_vertices[v1].seed;
	if (nseed == 0)
		nseed = 497137451;
	int index = m_base_vertices.size();
	m_base_vertices.push_back({ { edge.f0, -1, -1, -1 }, {edge.f1, -1, -1, -1 }, nseed, 0, 0, 0 });
	edge.vm = index;

	const double levelf = math::clamp((double)level, 0.0, 8.0) / 8.0;
	math::dvec3 p0(m_base_vattrib[v0].position);
	math::dvec3 p1(m_base_vattrib[v1].position);
	math::dvec3 p;

	//if (edge.type != TYPE_OCTAHEDRON_EDGE)
	//{
		int va, vb;
		getSurroundingVertices(edge_index, edge, va, vb);
		const double displacement_amplitude = 0.2 * levelf*levelf;
		const double rx = displacement_amplitude * (double)(std::rand() % 65536) / 65535.0;
		const double r0 = (1.0 - rx) * (0.5 - displacement_amplitude + displacement_amplitude * (double)(std::rand() % 65536) / 65535.0);
		const double r1 = 1.0 - rx - r0;
		int vx = va;
		if ((std::rand() % 128) < 64)
			vx = vb;
		math::dvec3 px(m_base_vattrib[vx].position);
		p = rx * px + r0 * p0 + r1 * p1;//barycentric displacement ensures new position lies within the neighboring triangle.
	//}
	//else 
	//{
	//	p = math::mix(p0, p1, 0.5);
	//}
	
	p = math::normalize(p);
	
	const PlanetData::Data data = m_planet->getInterpolatedModelData(p);
	const float plateaux = m_planet->getPlateauxDistribution(p);
	const float desert = m_planet->getDesertDistribution(p);
	const float hills = m_planet->getHillsDistribution(p);

	double r = (double)(std::rand() % 65536) / 65535.0;
	double elevation = m_planet->seaLevelKm + (data.elevation - m_planet->seaLevelKm) * (0.7 + 0.3*r);// math::mix(0.4 + 0.6*r, 0.88 - 0.2*r, (double)plateaux); // RANDOMIZE (OR NOT)
	if (data.elevation <= m_planet->seaLevelKm)
		elevation = data.elevation;
	p *= m_planet->radiusKm + elevation;
	const float tectonic_age = (float)(data.age);	
	m_base_vattrib.push_back({
		  math::dvec4(p, elevation)
		, math::dvec4(m_planet->seaLevelKm // nearest river altitude : ad hoc value
						, 50.0 // distance to river: ad hoc value 
						, data.elevation // max crust elevation 
						, m_planet->seaLevelKm) // water altitude : ad hoc value
		, math::vec4(0.0f, 0.0f, 0.0f, 0.0f) // water flow : ad hoc value
		, math::vec4((float)(m_planet->seaLevelKm) // nearest ravin altitude : ad hoc value
						, 50.0f // distance to ravin : ad hoc value 
						, hills, 0.0f)
		, math::vec4(tectonic_age, plateaux, desert, 0.0f)
		, math::vec4(0.0)
	});
	
	if (elevation > m_planet->seaLevelKm)
		m_base_vertices[index].type = TYPE_CONTINENT;
	else m_base_vertices[index].type = TYPE_SEA;

	// create 2 subedges
	int k = m_base_edges.size();
	unsigned int substatus = (level + 1) << 16;
	unsigned int type = TYPE_NONE;
	if (edge.type == TYPE_OCTAHEDRON_EDGE)
		type = TYPE_OCTAHEDRON_EDGE;
	m_base_edges.push_back({ edge.v0, edge.vm, -1, substatus, -1, -1, edge.f0, edge.f1, type });
	m_base_edges.push_back({ edge.vm, edge.v1, -1, substatus, -1, -1, edge.f0, edge.f1, type });
	edge.child0 = k;
	edge.child1 = k + 1;
}
*/

bool RenderablePlanet::isTriangleSeaCoast(int triangle_index) const
{
	int countSea = 0;
	const TriangleGPU & T = m_base_triangles[triangle_index];
	const EdgeGPU & e0 = m_base_edges[T.e0];
	const EdgeGPU & e1 = m_base_edges[T.e1];
	const EdgeGPU & e2 = m_base_edges[T.e2];

	bool flip0 = false;
	bool flip1 = false;
	bool flip2 = false;//determine winding of edges (as they can appear in any order)
	if (e0.v0 == e1.v0 || e0.v0 == e1.v1)
		flip0 = true;
	if (e1.v0 == e2.v0 || e1.v0 == e2.v1)
		flip1 = true;
	if (e2.v0 == e0.v0 || e2.v0 == e0.v1)
		flip2 = true;
	int v0 = (flip0 ? e0.v1 : e0.v0);
	int v1 = (flip1 ? e1.v1 : e1.v0);
	int v2 = (flip2 ? e2.v1 : e2.v0);
	
	std::list<int> list;
	list.push_back(v0);
	list.push_back(v1);
	list.push_back(v2);
	std::set<int> visited;
	std::vector<int> edges;
	edges.reserve(16);
	
	while (!list.empty())
	{
		int v = list.front();
		list.pop_front();
		if (visited.find(v) != visited.end())
			continue;
		visited.insert(v);

		edges.clear();
		getAdjacentEdges(v, edges);

		for (int e : edges)
		{
			const EdgeGPU &  edge = m_base_edges[e];
			int v2 = edge.v0 == v ? edge.v1 : edge.v0;
			if (visited.find(v2) != visited.end())
				continue;
			if (m_base_vertices[v2].type == TYPE_SEA)
			{
				countSea++;
				list.push_back(v2);
			}
		}
		if (countSea > 8)
			return true;
	}

	return countSea > 8;
}

void RenderablePlanet::createAllRiverMouth(std::vector<RiverGrowingNode> & nodes)
{
	std::mt19937 prng;
	std::vector<int> coasts;

	// -- assign triangle types and save aside coast triangles --
	for (int i = 0; i < m_base_triangles.size(); ++i)
	{
		TriangleGPU & T = m_base_triangles[i];
		if ((T.status & 0xFF) != 0)
			continue;

		EdgeGPU & e0 = m_base_edges[T.e0];
		EdgeGPU & e1 = m_base_edges[T.e1];
		EdgeGPU & e2 = m_base_edges[T.e2];

		bool flip0 = false;
		bool flip1 = false;
		bool flip2 = false;//determine winding of edges (as they can appear in any order)
		if (e0.v0 == e1.v0 || e0.v0 == e1.v1)
			flip0 = true;
		if (e1.v0 == e2.v0 || e1.v0 == e2.v1)
			flip1 = true;
		if (e2.v0 == e0.v0 || e2.v0 == e0.v1)
			flip2 = true;
		int v0 = (flip0 ? e0.v1 : e0.v0);
		int v1 = (flip1 ? e1.v1 : e1.v0);
		int v2 = (flip2 ? e2.v1 : e2.v0);
		VertexGPU & V0 = m_base_vertices[v0];
		VertexGPU & V1 = m_base_vertices[v1];
		VertexGPU & V2 = m_base_vertices[v2];

		int count = 0;
		if (V0.type == TYPE_SEA)
			count++;
		if (V1.type == TYPE_SEA) // sea vertex
			count++;
		if (V2.type == TYPE_SEA) // sea vertex
			count++;
		if (count == 0)
		{
			T.type = TYPE_CONTINENT;
			continue;
		}
		if (count == 3)
		{
			T.type = TYPE_SEA;
			continue;
		}

		T.type = TYPE_COAST;
		if (count == 2)
			coasts.push_back(i);
	}

	// -- make clean coast triangles (ie., without any sea vertex) --
	for (int t : coasts)
	{
		TriangleGPU & T = m_base_triangles[t];
		EdgeGPU & e0 = m_base_edges[T.e0];
		EdgeGPU & e1 = m_base_edges[T.e1];
		EdgeGPU & e2 = m_base_edges[T.e2];

		bool flip0 = false;
		bool flip1 = false;
		bool flip2 = false;//determine winding of edges (as they can appear in any order)
		if (e0.v0 == e1.v0 || e0.v0 == e1.v1)
			flip0 = true;
		if (e1.v0 == e2.v0 || e1.v0 == e2.v1)
			flip1 = true;
		if (e2.v0 == e0.v0 || e2.v0 == e0.v1)
			flip2 = true;
		int v0 = (flip0 ? e0.v1 : e0.v0);
		int v1 = (flip1 ? e1.v1 : e1.v0);
		int v2 = (flip2 ? e2.v1 : e2.v0);
		VertexGPU & V0 = m_base_vertices[v0];
		VertexGPU & V1 = m_base_vertices[v1];
		VertexGPU & V2 = m_base_vertices[v2];

		int cv;
		if (V0.type == TYPE_CONTINENT)
			cv = v0;
		else if (V1.type == TYPE_CONTINENT)
			cv = v1;
		else cv = v2;
		const double ref_altitude = m_base_vattrib[cv].position.w;

		if (V0.type == TYPE_SEA)
		{
			V0.type = TYPE_COAST;
			math::dvec4 va = m_base_vattrib[v0].position;
			math::dvec4 vd = m_base_vattrib[v0].data;
			m_base_vattrib[v0].data = math::dvec4(vd.x, vd.y, ref_altitude, vd.w);
			math::dvec3 p(va);
			p = normalize(p) * (m_planet->radiusKm + ref_altitude);
			m_base_vattrib[v0].position = math::dvec4(p, ref_altitude);			
		}
		if (V1.type == TYPE_SEA)
		{
			V1.type = TYPE_COAST;
			math::dvec4 va = m_base_vattrib[v1].position;
			math::dvec4 vd = m_base_vattrib[v1].data;
			m_base_vattrib[v1].data = math::dvec4(vd.x, vd.y, ref_altitude, vd.w);
			math::dvec3 p(va);
			p = normalize(p) * (m_planet->radiusKm + ref_altitude);
			m_base_vattrib[v1].position = math::dvec4(p, ref_altitude);
		}
		if (V2.type == TYPE_SEA)
		{
			V2.type = TYPE_COAST;
			math::dvec4 va = m_base_vattrib[v2].position;
			math::dvec4 vd = m_base_vattrib[v2].data;
			m_base_vattrib[v2].data = math::dvec4(vd.x, vd.y, ref_altitude, vd.w);
			math::dvec3 p(va);
			p = normalize(p) * (m_planet->radiusKm + ref_altitude);
			m_base_vattrib[v2].position = math::dvec4(p, ref_altitude);
		}
	}

	// -- create starting point for rivers (river mouth) --
	std::vector<int> adjacency;
	adjacency.reserve(16);
	for (int i : coasts)
	{
		TriangleGPU & T = m_base_triangles[i];
		EdgeGPU & e0 = m_base_edges[T.e0];
		EdgeGPU & e1 = m_base_edges[T.e1];
		EdgeGPU & e2 = m_base_edges[T.e2];

		bool flip0 = false;
		bool flip1 = false;
		bool flip2 = false;//determine winding of edges (as they can appear in any order)
		if (e0.v0 == e1.v0 || e0.v0 == e1.v1)
			flip0 = true;
		if (e1.v0 == e2.v0 || e1.v0 == e2.v1)
			flip1 = true;
		if (e2.v0 == e0.v0 || e2.v0 == e0.v1)
			flip2 = true;
		int v0 = (flip0 ? e0.v1 : e0.v0);
		int v1 = (flip1 ? e1.v1 : e1.v0);
		int v2 = (flip2 ? e2.v1 : e2.v0);
		VertexGPU & V0 = m_base_vertices[v0];
		VertexGPU & V1 = m_base_vertices[v1];
		VertexGPU & V2 = m_base_vertices[v2];
		
		if (V0.type == TYPE_RIVER || V1.type == TYPE_RIVER || V2.type == TYPE_RIVER)
			continue;

		double a0 = m_base_vattrib[v0].position.w - m_planet->seaLevelKm;
		double a1 = m_base_vattrib[v1].position.w - m_planet->seaLevelKm;
		double a2 = m_base_vattrib[v2].position.w - m_planet->seaLevelKm;
		//if (a0 > 1.7 && a1 > 1.7 && a2 > 1.7)// make sure mountainous coast (1700 m) don't become river mouth
			//continue;

		if (V0.type == TYPE_COAST)
		{// make sure no neighbor vertex is of type River:
			bool nogood = false;
			adjacency.clear();
			getAdjacentEdges(v0, adjacency);
			for (int e : adjacency)
			{
				const EdgeGPU & edge = m_base_edges[e];
				int w = edge.v0 == v0 ? edge.v1 : edge.v0;
				if (m_base_vertices[w].type == TYPE_RIVER)
				{
					nogood = true;
					break;
				}
			}
			if (nogood)
				continue;
		}
		if (V1.type == TYPE_COAST)
		{// make sure no neighbor vertex is of type River:
			bool nogood = false;
			adjacency.clear();
			getAdjacentEdges(v1, adjacency);
			for (int e : adjacency)
			{
				const EdgeGPU & edge = m_base_edges[e];
				int w = edge.v0 == v1 ? edge.v1 : edge.v0;
				if (m_base_vertices[w].type == TYPE_RIVER)
				{
					nogood = true;
					break;
				}
			}
			if (nogood)
				continue;
		}
		if (V2.type == TYPE_COAST)
		{// make sure no neighbor vertex is of type River:
			bool nogood = false;
			adjacency.clear();
			getAdjacentEdges(v2, adjacency);
			for (int e : adjacency)
			{
				const EdgeGPU & edge = m_base_edges[e];
				int w = edge.v0 == v2 ? edge.v1 : edge.v0;
				if (m_base_vertices[w].type == TYPE_RIVER)
				{
					nogood = true;
					break;
				}
			}
			if (nogood)
				continue;
		}
		
		if (!isTriangleSeaCoast(i))
			continue;

		RiverGrowingNode node;
		int n0 = e0.f0 == i ? e0.f1 : e0.f0;
		int n1 = e1.f0 == i ? e1.f1 : e1.f0;
		int n2 = e2.f0 == i ? e2.f1 : e2.f0;
		int ne, nv0, nv1;
		math::dvec3 p;
		int mouth_index;

		if (m_base_triangles[n0].type == TYPE_SEA && m_base_triangles[n1].type != TYPE_SEA && m_base_triangles[n2].type != TYPE_SEA)
		{
			if (V1.type == TYPE_COAST)
			{
				ne = T.e1;
				nv0 = v2;
				nv1 = v1;
			}
			else {
				ne = T.e2;
				nv0 = v2;
				nv1 = v0;
			}
			node.edge = ne;
			node.tip_vertex = nv0;			

			// check that, indeed, the candidate river mouth has a connexion to the sea
			bool has_sea_outlet = false;
			adjacency.clear();
			getAdjacentEdges(nv1, adjacency);
			for (int e : adjacency)
			{
				const EdgeGPU & edge = m_base_edges[e];
				int w = edge.v0 == nv1 ? edge.v1 : edge.v0;
				if (m_base_vertices[w].type == TYPE_SEA)
				{
					has_sea_outlet = true;
					break;
				}
			}
			if (!has_sea_outlet)
				continue;

			// river mouth:
			mouth_index = nv1;			
		}
		else if (m_base_triangles[n1].type == TYPE_SEA && m_base_triangles[n0].type != TYPE_SEA && m_base_triangles[n2].type != TYPE_SEA)
		{
			if (V1.type == TYPE_COAST)
			{
				ne = T.e0;
				nv0 = v0;
				nv1 = v1;
			}
			else {
				ne = T.e2;
				nv0 = v0;
				nv1 = v2;
			}
			node.edge = ne;
			node.tip_vertex = nv0;

			// check that, indeed, the candidate river mouth has a connexion to the sea
			bool has_sea_outlet = false;
			adjacency.clear();
			getAdjacentEdges(nv1, adjacency);
			for (int e : adjacency)
			{
				const EdgeGPU & edge = m_base_edges[e];
				int w = edge.v0 == nv1 ? edge.v1 : edge.v0;
				if (m_base_vertices[w].type == TYPE_SEA)
				{
					has_sea_outlet = true;
					break;
				}
			}
			if (!has_sea_outlet)
				continue;

			// river mouth:
			mouth_index = nv1;
		}
		else if (m_base_triangles[n2].type == TYPE_SEA && m_base_triangles[n1].type != TYPE_SEA && m_base_triangles[n0].type != TYPE_SEA)
		{
			if (V0.type == TYPE_COAST)
			{
				ne = T.e0;
				nv0 = v1;
				nv1 = v0;
			}
			else {
				ne = T.e1;
				nv0 = v1;
				nv1 = v2;
			}
			node.edge = ne;
			node.tip_vertex = nv0;

			// check that, indeed, the candidate river mouth has a connexion to the sea
			bool has_sea_outlet = false;
			adjacency.clear();
			getAdjacentEdges(nv1, adjacency);
			for (int e : adjacency)
			{
				const EdgeGPU & edge = m_base_edges[e];
				int w = edge.v0 == nv1 ? edge.v1 : edge.v0;
				if (m_base_vertices[w].type == TYPE_SEA)
				{
					has_sea_outlet = true;
					break;
				}
			}
			if (!has_sea_outlet)
				continue;

			// river mouth:
			mouth_index = nv1;			
		}
		else
			continue;

		math::dvec4 va = m_base_vattrib[node.tip_vertex].position;
		double r = (double)(prng() % 65536) / 65535.0;
		double altitude_tip = (0.99 + 0.005 * r) * m_planet->seaLevelKm;//bottom of river is somewhere in ] -100m, -50m]
		math::dvec3 p_tip = (m_planet->radiusKm + altitude_tip) * math::normalize(math::dvec3(va));

		node.num_edges_to_mouth = 1.0;
		node.length_to_mouth = 0.0;
		r = (double)(prng() % 65536) / 65535.0;
		r = std::pow(r, 0.75);
		node.target_river_length = std::max(500.0, r * MAX_RIVER_LENGTH);
		node.spring = false;
		node.full_grown = false;
		node.normalized_tecto_altitude = (m_base_vattrib[node.tip_vertex].data.z - m_planet->seaLevelKm)/ m_planet->maxAltitude;
		node.priority = 1.0f;// r;
		node.base_vertex = mouth_index;		
		const float flowvalue = 0.1f + 0.9f * (float)r * (0.618f + 0.382f * (prng() % 65536) / 65535.0f);

		m_base_vertices[mouth_index].type = TYPE_RIVER;
		va = m_base_vattrib[mouth_index].position;
		r = (double)(prng() % 65536) / 65535.0;
		double altitude_mouth = (0.98 + 0.01 * r) * m_planet->seaLevelKm;// bottom of river mouth is somewhere in ]-100m, -200m]
		p = (m_planet->radiusKm + altitude_mouth) * math::normalize(math::dvec3(va));
		m_base_vattrib[mouth_index].position = math::dvec4(p, altitude_mouth);
		m_base_vattrib[mouth_index].data.x = altitude_mouth;//river bed altitude
		m_base_vattrib[mouth_index].data.y = 0.0;//distance to river is 0
		math::vec3 flow_dir = math::vec3(math::normalize(p - p_tip));
		m_base_vattrib[mouth_index].flow = math::vec4(flow_dir, flowvalue);//normalized direction of the flow and normalized flow quantity
		m_base_vattrib[mouth_index].misc2.w = (float)r;//random river profile for now.
		//m_base_vattrib[mouth_index].padding_and_debug.w = (float)(node.target_river_length / MAX_RIVER_LENGTH);
		m_base_vattrib[mouth_index].padding_and_debug.w = (float)node.priority;

		m_base_edges[node.edge].type = TYPE_RIVER;
		m_base_vertices[node.tip_vertex].type = TYPE_RIVER;
		m_base_vattrib[node.tip_vertex].position = math::dvec4(p_tip, altitude_tip);
		m_base_vattrib[node.tip_vertex].data.x = altitude_tip;
		m_base_vattrib[node.tip_vertex].data.y = 0.0;		
		m_base_vattrib[node.tip_vertex].flow = math::vec4(flow_dir, flowvalue);
		m_base_vattrib[node.tip_vertex].misc2.w = m_base_vattrib[mouth_index].misc2.w;
		//m_base_vattrib[node.tip_vertex].padding_and_debug.w = (float)(node.target_river_length / MAX_RIVER_LENGTH);
		m_base_vattrib[node.tip_vertex].padding_and_debug.w = (float)node.priority;
		node.max_flow_value = flowvalue;
		nodes.push_back(node);
	}
}

static bool compareBaseRiverNodeIteratorStochastic(const std::list<RiverGrowingNode>::iterator & A, const std::list<RiverGrowingNode>::iterator & B, double randomVal)
{
	return (A->priority * (1.0 - A->normalized_tecto_altitude) * (0.5 + 0.5*randomVal)) > (B->priority * (1.0 - B->normalized_tecto_altitude));
}

static bool compareBaseRiverNodeIterator(const std::list<RiverGrowingNode>::iterator & A, const std::list<RiverGrowingNode>::iterator & B)
{
	return (A->priority * (1.0 - A->normalized_tecto_altitude)) > (B->priority * (1.0 - B->normalized_tecto_altitude));
}

static bool compareBaseRiverNode(const RiverGrowingNode & A, const RiverGrowingNode & B)
{
	return A.choice_penalty < B.choice_penalty;
}

void RenderablePlanet::createBaseRiverNetwork()
{
	MAX_RIVER_LENGTH = m_planet->radiusKm * 1.6;

	std::mt19937 prng;
	std::vector<RiverGrowingNode> mouths;
	
	// -- track elevated base vertices (aka mountain vertices) --
#ifdef BASE_RIVERS_LOOKUP_TECTONIC_ELEVATIONS
	struct MountainVertex
	{
		math::dvec4 position;
		int index;
	};
	std::vector<MountainVertex> mountains;
	mountains.reserve((int)std::sqrt((double)m_base_vertices.size()));
	for (int i = 0; i < m_base_vertices.size(); ++i)
	{
		math::dvec4 p = m_base_vattrib[i].position;
		if (p.w > m_planet->seaLevelKm + 1.0) // above 1000m altitude
		{
			mountains.push_back({ p, i });
		}
	}
#endif

	// -- assign all river mouth (and make clear cut coasts) -- 
	createAllRiverMouth(mouths);
	std::list<RiverGrowingNode> nodes(mouths.begin(), mouths.end());

	std::vector<RiverGrowingNode> candidates;
	candidates.reserve(16);
	std::vector<int> adjacency, tip_adjacency;
	adjacency.reserve(16);
	tip_adjacency.reserve(16);

	std::vector<RiverGrowingNode> branch_list;
	branch_list.reserve(mouths.size() * 32);
	std::vector<std::list<RiverGrowingNode>::iterator> to_delete;
	to_delete.reserve(mouths.size() * 32);
	std::vector<std::list<RiverGrowingNode>::iterator> sorted_node_iterators;
	sorted_node_iterators.reserve(mouths.size() * 32);

	m_river_nodes_max_size = m_base_vertices.size();
	m_river_nodes = new RiverNode[m_river_nodes_max_size];
	m_river_nodes_size = 0;
	
	int c = 0;
	for (auto it = nodes.begin(); it != nodes.end(); ++it)
	{
		m_rivers.push_back(m_river_nodes_size);

		m_river_nodes[m_river_nodes_size] = { 
			it->base_vertex,
			-1,
			m_river_nodes_size + 1, -1,
			it->edge, -1,
			0.0f,
			-1, 
			-1.0f,
			-1
			, 0.0,
			c, //river_system_id
			false
			};		
		m_river_nodes_size++;
		
		it->tip_river_node = m_river_nodes_size;

		m_river_nodes[m_river_nodes_size] = {
			it->tip_vertex,
			m_river_nodes_size - 1,
			-1, -1,
			-1, -1, 
			0.0f,
			-1,
			-1.0f,
			-1
			, 0.0,
			c, //river_system_id
			false
		};
		m_river_nodes_size++;

		c++;
	}

	// -- grow rivers --	
	int MAX_NODES_TO_PROCESS = 8;
	bool added;
	do
	{
		added = false;
		if (nodes.empty())
			break;

		sorted_node_iterators.clear();
		for (auto it = nodes.begin(); it != nodes.end(); ++it)
			sorted_node_iterators.push_back(it);
		std::sort(sorted_node_iterators.begin(), sorted_node_iterators.end(), compareBaseRiverNodeIterator);
				
		int processed_nodes = 0;
		for (auto it = sorted_node_iterators.begin(); it != sorted_node_iterators.end(); ++it)//try to grow each node
		{
			const RiverGrowingNode & node = **it;
			if (node.spring)
				continue;

			const EdgeGPU & edge = m_base_edges[node.edge];
			const int v = node.tip_vertex;
			VertexGPU & V = m_base_vertices[v];
			const math::dvec3 pv(m_base_vattrib[v].position);
			const math::dvec3 edgevec = pv - math::dvec3(m_base_vattrib[edge.v0 == v ? edge.v1 : edge.v0].position);
			const PlanetData::Data local_data = m_planet->getInterpolatedModelData(pv);

			bool edgefound = false;
			adjacency.clear();
			getAdjacentEdges(v, adjacency);

			candidates.clear();
			RiverGrowingNode candidateNode;

			for (int k = 0; k < adjacency.size(); ++k)
			{
				const int e = adjacency[k];
				const EdgeGPU & candidateEdge = m_base_edges[e];
				const int w = (candidateEdge.v0 == v ? candidateEdge.v1 : candidateEdge.v0);

				if (m_base_vertices[w].type != TYPE_CONTINENT)
					continue;
				if (m_base_triangles[candidateEdge.f0].type != TYPE_CONTINENT || m_base_triangles[candidateEdge.f1].type != TYPE_CONTINENT)
					continue;

				const math::dvec3 edgevec2 = math::dvec3(m_base_vattrib[w].position) - pv;
				const double dotEdges = dot(edgevec, edgevec2);
				if (dotEdges < 0.2)
					continue;//check angle of edges to make proper rivers

				// check that all edges adjacent to the candidate tip vertex are NOT themselves adjacent to a coast triangle (to prevent river springs near the coast).
				tip_adjacency.clear();
				getAdjacentEdges(w, tip_adjacency);
				bool tip_adjacency_ok = true;
				for (int t : tip_adjacency)
				{
					const EdgeGPU & tip_edge = m_base_edges[t];
					int w2 = tip_edge.v0 == w ? tip_edge.v1 : tip_edge.v0;
					if (m_base_vertices[w2].type == TYPE_SEA || m_base_triangles[tip_edge.f0].type != TYPE_CONTINENT || m_base_triangles[tip_edge.f1].type != TYPE_CONTINENT)
					{
						tip_adjacency_ok = false;
						//edgefound = false;
						break;
					}
				}
				if (!tip_adjacency_ok)
					continue;

				edgefound = true;
				RiverGrowingNode candidate;
				candidate.edge = e;
				candidate.tip_vertex = w;
				double penalty = 100.0;
#ifdef BASE_RIVERS_LOOKUP_TECTONIC_ELEVATIONS
				for (const MountainVertex & MV : mountains)
				{
					double distance = math::distance(math::dvec3(MV.position), math::dvec3(m_base_vattrib[w].position));
					distance /= std::sqrt(MV.position.w);//the higher the mountain the more weight it has.
					if (interest > distance)
						interest = distance;
				}
#endif
				double r = (double)(prng() % 65536) / 65535.0;
				penalty *= 0.1 + 0.38*r + std::abs(math::dot(local_data.strain_direction, math::normalize(edgevec2)));// favor river direction orthogonal to local tectonic folding direction
				r = (double)(prng() % 65536) / 65535.0;
				//penalty *= 0.38*r + (1.0 - dotEdges);//favor non sinuosity
				penalty *= (0.05 + m_base_vattrib[w].misc2.y);//favor non-plateaux to grow river locally 
				penalty *= 1.0 - 0.99*math::smoothstep(-0.2, 0.1, m_base_vattrib[w].data.z - m_base_vattrib[v].data.z);//favor going "up hill"
				candidate.choice_penalty = penalty;
				candidates.push_back(candidate);				
			}

			if (!edgefound)
			{// cannot grow river: terminate it and make a local spring.
				(*it)->spring = true;
				const double water_altitude = m_base_vattrib[v].position.w;
				m_base_vattrib[v].data.w = water_altitude;
				m_base_vattrib[v].flow.w = SPRING_FLOWVALUE;
				m_base_vertices[v].type = TYPE_RIVER;				
				continue;
			}
				
			// sort candidates based on interest:
			std::sort(candidates.begin(), candidates.end(), compareBaseRiverNode);
			candidateNode = candidates[0];				
			
			bool branching = false;
			RiverGrowingNode branch;
			
			const double proba_lerp = math::clamp(node.length_to_mouth / MAX_RIVER_LENGTH, 0.0, 1.0);
			const int branching_proba = (int)math::mix(100.0, 0.0, std::sqrt(proba_lerp));

			if (edgefound && prng() % 100 >= branching_proba && node.num_edges_to_mouth > 2.0)// sometimes make a branching river node (except for river mouth or so)
			{
				candidates.clear();

				for (int k = 0; k < adjacency.size(); ++k)
				{
					const int e = adjacency[k];
					if (e == candidateNode.edge)
						continue;

					const EdgeGPU & candidateEdge = m_base_edges[e];
				
					const int w = (candidateEdge.v0 == v ? candidateEdge.v1 : candidateEdge.v0);
					if (m_base_vertices[w].type != TYPE_CONTINENT)
						continue;
					if (m_base_triangles[candidateEdge.f0].type != TYPE_CONTINENT || m_base_triangles[candidateEdge.f1].type != TYPE_CONTINENT)
						continue;
					const math::dvec3 edgevec2 = math::dvec3(m_base_vattrib[w].position) - pv;
					if (dot(edgevec, edgevec2) < 0.0)
						continue;//check angle of edges to make proper rivers

					tip_adjacency.clear();
					getAdjacentEdges(w, tip_adjacency);
					bool tip_adjacency_ok = true;
					for (int t : tip_adjacency)
					{
						const EdgeGPU & tip_edge = m_base_edges[t];
						int w2 = tip_edge.v0 == w ? tip_edge.v1 : tip_edge.v0;
						if (m_base_vertices[w2].type == TYPE_SEA || m_base_triangles[tip_edge.f0].type != TYPE_CONTINENT || m_base_triangles[tip_edge.f1].type != TYPE_CONTINENT)
						{
							tip_adjacency_ok = false;
							break;
						}
					}
					if (!tip_adjacency_ok)
						continue;

					branching = true;
					branch.edge = e;
					branch.tip_vertex = w;
					double penalty = 100.0;
					double r = (double)(prng() % 65536) / 65535.0;
					penalty *= 0.1 + 0.38*r + std::abs(math::dot(local_data.strain_direction, math::normalize(edgevec2)));// favor river direction orthogonal to local tectonic folding direction
					r = (double)(prng() % 65536) / 65535.0;
					//penalty *= 0.38*r + (1.0 - dotEdges);//favor non sinuosity
					penalty *= (0.05 + m_base_vattrib[w].misc2.y);//favor non-plateaux to grow river locally 
					penalty *= 1.0 - 0.99*math::smoothstep(-0.2, 0.1, m_base_vattrib[w].data.z - m_base_vattrib[v].data.z);//favor going "up hill"
					branch.choice_penalty = penalty;

					candidates.push_back(branch);					
				}

				std::sort(candidates.begin(), candidates.end(), compareBaseRiverNode);
				branch = candidates[0];
			}

			double nl = node.num_edges_to_mouth + 1.0;
			const double prev_altitude = m_base_vattrib[v].position.w;//altitude of the previous river vertex			
			const float prev_riverprofile = m_base_vattrib[v].misc2.w;

			double MAX_SPRING_ALTITUDE;
			const double Margin = 0.07;// 70 m
			
			if (edgefound)
			{
				added = true;

				candidateNode.base_vertex = node.tip_vertex;
				candidateNode.priority = node.priority;
				
				math::dvec4 va = m_base_vattrib[candidateNode.tip_vertex].position;

				candidateNode.num_edges_to_mouth = nl;
				candidateNode.spring = false;
				candidateNode.length_to_mouth = node.length_to_mouth + math::distance(math::dvec3(va), pv);
				candidateNode.normalized_tecto_altitude = (m_base_vattrib[candidateNode.tip_vertex].data.z - m_planet->seaLevelKm) / m_planet->maxAltitude;
				
				m_base_edges[candidateNode.edge].type = TYPE_RIVER;
				m_base_vertices[candidateNode.tip_vertex].type = TYPE_RIVER;
				
				const double max_altitude = va.w;		
				double MAXALT = max_altitude - Margin;
				if (MAXALT < m_planet->seaLevelKm + 0.008)
					MAXALT = max_altitude;
				MAX_SPRING_ALTITUDE = std::max(0.7, 0.5 * (MAXALT - m_planet->seaLevelKm)) + m_planet->seaLevelKm;
				double r = (double)(prng() % 65536) / 65535.0;
				double altitude = prev_altitude + std::max(0.0, r*r * (max_altitude - m_planet->seaLevelKm) * 0.02);//max 200 m
				if (node.num_edges_to_mouth == 1.0)
					altitude = m_planet->seaLevelKm;
				altitude = std::max(altitude, m_planet->seaLevelKm - 0.02);
				
				math::dvec3 p;
				
				if (altitude < MAXALT && altitude < MAX_SPRING_ALTITUDE && candidateNode.length_to_mouth < MAX_RIVER_LENGTH)
				{//only grow the river if conditions are met
					p = math::dvec3(va);
					p = math::normalize(p) * (m_planet->radiusKm + altitude);
					const double water_altitude = altitude;
					m_base_vattrib[candidateNode.tip_vertex].position = math::dvec4(p, altitude);										
					m_base_vattrib[candidateNode.tip_vertex].data = math::dvec4(altitude, 0.0, max_altitude, water_altitude);								
				}
				else // else make the river spring and terminate
				{
					altitude = std::max(prev_altitude, std::min(MAX_SPRING_ALTITUDE, MAXALT));
					p = math::dvec3(va);
					p = math::normalize(p) * (m_planet->radiusKm + altitude);
					const double water_altitude = altitude;
					m_base_vattrib[candidateNode.tip_vertex].position = math::dvec4(p, altitude);
					m_base_vattrib[candidateNode.tip_vertex].data = math::dvec4(altitude, 0.0, max_altitude, water_altitude);
					candidateNode.spring = true;
				}				
				m_base_vattrib[candidateNode.tip_vertex].flow = math::vec4(math::vec3(math::normalize(pv - p)), 0.0f);
				m_base_vattrib[candidateNode.tip_vertex].misc2.w = prev_riverprofile + 0.05f * (float)(prng() % 65536) / 65535.0f;//random offset from previous vertex for river profile
				m_base_vattrib[candidateNode.tip_vertex].padding_and_debug.w = (float)(candidateNode.length_to_mouth / MAX_RIVER_LENGTH);
				
				candidateNode.tip_river_node = m_river_nodes_size;
				m_river_nodes[node.tip_river_node].nextnode1 = m_river_nodes_size;
				m_river_nodes[node.tip_river_node].nextedge1 = candidateNode.edge;
				m_river_nodes[m_river_nodes_size] = {
					candidateNode.tip_vertex,
					node.tip_river_node,
					-1, -1,
					-1, -1, 
					0.0f,
					-1,
					-1.0f,
					-1,
					candidateNode.length_to_mouth,
					m_river_nodes[node.tip_river_node].river_system_id,
					false
				};
				m_river_nodes_size++;

				**it = candidateNode;																	
			}

			if (branching)
			{
				math::dvec4 va = m_base_vattrib[branch.tip_vertex].position;
				V.branch_count = 1;

				branch.base_vertex = node.tip_vertex;
				branch.length_to_mouth = node.length_to_mouth + math::distance(math::dvec3(va), pv);
				branch.priority = node.priority;
				
				branch.num_edges_to_mouth = nl;
				branch.spring = false;
				branch.normalized_tecto_altitude = (m_base_vattrib[branch.tip_vertex].data.z - m_planet->seaLevelKm) / m_planet->maxAltitude;
				m_base_edges[branch.edge].type = TYPE_RIVER;
				m_base_vertices[branch.tip_vertex].type = TYPE_RIVER;
								
				const double max_altitude = va.w;
				double MAXALT = max_altitude - Margin;
				if (MAXALT < m_planet->seaLevelKm + 0.008)
					MAXALT = max_altitude;
				MAX_SPRING_ALTITUDE = std::max(0.7, 0.7 * (MAXALT - m_planet->seaLevelKm)) + m_planet->seaLevelKm;
				double r = (double)(prng() % 65536) / 65535.0;
				double altitude = prev_altitude + std::max(0.0, (1.0 - r) * (max_altitude - m_planet->seaLevelKm) * 0.02);//max 200m
				if (node.num_edges_to_mouth == 1.0)
					altitude = m_planet->seaLevelKm;
				altitude = std::max(altitude, m_planet->seaLevelKm - 0.02);

				math::dvec3 p;
				
				if (altitude < MAXALT && altitude < MAX_SPRING_ALTITUDE && branch.length_to_mouth < MAX_RIVER_LENGTH)
				{//only grow the river if altitude stays below planet data
					p = math::dvec3(va);
					p = math::normalize(p) * (m_planet->radiusKm + altitude);
					const double water_altitude = altitude;
					m_base_vattrib[branch.tip_vertex].position = math::dvec4(p, altitude);
					m_base_vattrib[branch.tip_vertex].data = math::dvec4(altitude, 0.0, max_altitude, water_altitude);					
				}
				else // else make the river spring and terminate
				{
					altitude = std::max(prev_altitude, std::min(MAX_SPRING_ALTITUDE, MAXALT));
					p = math::dvec3(va);
					p = math::normalize(p) * (m_planet->radiusKm + altitude);
					const double water_altitude = altitude;
					m_base_vattrib[branch.tip_vertex].position = math::dvec4(p, altitude);
					m_base_vattrib[branch.tip_vertex].data = math::dvec4(altitude, 0.0, max_altitude, water_altitude);
					branch.spring = true;
				}
				m_base_vattrib[branch.tip_vertex].flow = math::vec4(math::vec3(math::normalize(pv - p)), 0.0);
				m_base_vattrib[branch.tip_vertex].misc2.w = prev_riverprofile + 0.05f * (float)(prng() % 65536) / 65535.0f;//random offset from previous vertex for river profile
				m_base_vattrib[branch.tip_vertex].padding_and_debug.w = (float)(branch.length_to_mouth / MAX_RIVER_LENGTH);
				
				branch.tip_river_node = m_river_nodes_size;
				if (!branch.spring)
				{					
					branch_list.push_back(branch);
				}

				m_river_nodes[node.tip_river_node].nextnode2 = m_river_nodes_size;
				m_river_nodes[node.tip_river_node].nextedge2 = branch.edge;
				m_river_nodes[m_river_nodes_size] = {
					branch.tip_vertex,
					node.tip_river_node,
					-1, -1,
					-1, -1, 
					0.0f,
					-1,
					-1.0f,
					-1,
					branch.length_to_mouth,
					m_river_nodes[node.tip_river_node].river_system_id,
					false

				};
				m_river_nodes_size++;
			}

			processed_nodes++;
			if (processed_nodes >= MAX_NODES_TO_PROCESS)
				break;
		}

		for (auto it = nodes.begin(); it != nodes.end(); ++it)
		{
			if (it->spring)//detect all spring nodes
				to_delete.push_back(it);
		}
		for (int i = 0; i < to_delete.size(); ++i)
			nodes.erase(to_delete[i]);//remove them from main list
		to_delete.clear();

		for (auto it = branch_list.begin(); it != branch_list.end(); ++it)
			nodes.push_back(*it);//finally add all branch nodes to main list
		branch_list.clear();

		MAX_NODES_TO_PROCESS = (int)std::max(8.0, (double)nodes.size() / 8.0);
	} 
	while (true);
}

static int computeHortonStralher(RiverNode * array, int node)
{//@returns the Hroton-Stralher number of this node

	if (node == -1)
		return 0;

	int HS1 = computeHortonStralher(array, array[node].nextnode1);
	int HS2 = computeHortonStralher(array, array[node].nextnode2);

	if (HS1 == 0)
	{
		array[node].horton_stralher = 1;		
	}
	else 
	{
		if (HS2 == 0)
		{
			array[node].horton_stralher = HS1;
		}
		else if (HS1 == HS2)
		{
			array[node].horton_stralher = HS1 + 1;
			array[node].asymetric_branching = false;
		}
		else
		{
			array[node].horton_stralher = HS1 > HS2 ? HS1 : HS2;
			array[node].asymetric_branching = true;
		}
	}
	
	return array[node].horton_stralher;
}

static void computeRiverFlow(RiverNode * array, int node, float flow)
{
	RiverNode & n = array[node];
	
	if (n.nextnode1 == -1)
		n.flow_value = SPRING_FLOWVALUE;
	else 	
	{
		n.flow_value = flow;

		if (n.nextnode2 == -1)
		{
			computeRiverFlow(array, n.nextnode1, flow);
		}
		else
		{
			if (n.asymetric_branching)
			{
				float w1 = (float)array[n.nextnode1].horton_stralher;
				float w2 = (float)array[n.nextnode2].horton_stralher;
				float f1 = flow * w1 / (w1 + w2);
				float f2 = flow * w2 / (w1 + w2);
				computeRiverFlow(array, n.nextnode1, f1);
				computeRiverFlow(array, n.nextnode2, f2);
			}
			else {
				computeRiverFlow(array, n.nextnode1, flow * 0.5f);
				computeRiverFlow(array, n.nextnode2, flow * 0.5f);
			}
		}
	}
}

double RenderablePlanet::assignWaterElevations(int node, double water_depth_at_mouth)
{
	if (node == -1)
		return 0.0;

	const RiverNode & n = m_river_nodes[node];
	if (n.nextnode1 == -1)//river spring:
	{
		m_base_vattrib[n.vertex].data.w = m_base_vattrib[n.vertex].position.w;
		return n.length_to_mouth;//return total river length
	}

	const double length1 = assignWaterElevations(n.nextnode1, water_depth_at_mouth);
	const double length2 = assignWaterElevations(n.nextnode2, water_depth_at_mouth);
	const double riverlength = std::max(length1, length2);
	
	double t = n.length_to_mouth / riverlength;
	t *= t;
	const double water_depth = water_depth_at_mouth * (1.0 - 0.9 * t * t);
	m_base_vattrib[n.vertex].data.w = m_base_vattrib[n.vertex].position.w + water_depth;

	return riverlength;
}

void RenderablePlanet::postprocessBaseRiverNetwork()
{
	// --- compute Horton-Stralher number ---
	int max_hs = 0;	
	for (auto it = m_rivers.begin(); it != m_rivers.end(); ++it)
	{
		int mouth = *it;

		int hs = computeHortonStralher(m_river_nodes, mouth);
		if (hs > max_hs)
			max_hs = hs;			
	}
		
	double max_len = 0.0;
	std::vector<double> maxriverlength(m_rivers.size(), 0.0);
	for (int i=0; i < m_river_nodes_size; ++i)
		if (m_river_nodes[i].nextnode1 == -1)
		{
			if (m_river_nodes[i].length_to_mouth > max_len)
				max_len = m_river_nodes[i].length_to_mouth;

			if (m_river_nodes[i].length_to_mouth > maxriverlength[m_river_nodes[i].river_system_id])
				maxriverlength[m_river_nodes[i].river_system_id] = m_river_nodes[i].length_to_mouth;
		}
	std::cout << "Max river length = " << max_len << " km" << std::endl;

	// --- prune un-grown river nodes ---
	std::mt19937 prng;

	const double river_length_threshold = 10.0;//km
	for (int i = 0; i < m_river_nodes_size; ++i)
	{
		RiverNode & n = m_river_nodes[i];
		if (maxriverlength[n.river_system_id] > river_length_threshold)
		{
			prng.seed(n.river_system_id * 33875999);
			float river_id = (float)(prng() % 65536);
			m_base_vattrib[n.vertex].padding_and_debug.w = river_id;
			continue;
		}

		n.disabled = true;

		m_base_vertices[n.vertex].type = TYPE_CONTINENT;
		VertexAttributesGPU & attrib = m_base_vattrib[n.vertex];
		math::dvec3 pos = math::normalize(math::dvec3(attrib.position));
		attrib.position = math::dvec4((attrib.data.z + m_planet->radiusKm) * pos, attrib.data.z);
		attrib.data = math::dvec4(attrib.data.z, 50.0, attrib.data.z, m_planet->seaLevelKm);

		if (n.nextedge1 != -1)
			m_base_edges[n.nextedge1].type = TYPE_NONE;
		if (n.nextedge2 != -1)
			m_base_edges[n.nextedge2].type = TYPE_NONE;
	}
	
	// Compute river flow, output Horton-Strahler data:
	float avg_hs = 0.0f;
	float final_num_rivers = 0.0f;
	for (auto it = m_rivers.begin(); it != m_rivers.end(); ++it)
	{
		int mouth = *it;

		if (m_river_nodes[mouth].disabled)
			continue;
		final_num_rivers += 1.0f;

		int hs = m_river_nodes[mouth].horton_stralher;
		avg_hs += (float)hs;

		float flow = std::sqrt(maxriverlength[m_river_nodes[mouth].river_system_id] / max_len);
		computeRiverFlow(m_river_nodes, mouth, flow);		
	}
	avg_hs /= final_num_rivers;
	std::cout << "Final river systems count = " << (int)final_num_rivers << " (out of total " << m_rivers.size() << " candidate systems)." << std::endl;
	std::cout << "Horton-Strahler: max " << max_hs << ", average " << avg_hs << "." << std::endl;

	for (int i = 0; i < m_river_nodes_size; ++i)
	{
		RiverNode & n = m_river_nodes[i];
		if (n.disabled)
			continue;

		if (m_base_vertices[n.vertex].type == TYPE_RIVER)
			m_base_vattrib[n.vertex].flow.w = n.flow_value;
	}

	// Assign water elevations:
	for (auto it = m_rivers.begin(); it != m_rivers.end(); ++it)
	{
		int mouth = *it;
		if (m_river_nodes[mouth].disabled)
			continue;

		int nextmouth = m_river_nodes[mouth].nextnode1;
				
		const double water_depth = m_planet->seaLevelKm - m_base_vattrib[m_river_nodes[nextmouth].vertex].position.w;

		assignWaterElevations(nextmouth, water_depth);
	}
}

void RenderablePlanet::getAdjacentEdges(int vertex_index, std::vector<int> & adjacency) const
{
	const VertexGPU & vertex = m_base_vertices[vertex_index];
	for (int k = 0; k < 4; ++k)
	{
		if (vertex.faces_0[k] == -1)
			continue;
		const TriangleGPU & T = m_base_triangles[vertex.faces_0[k]];
		
		const EdgeGPU & E0 = m_base_edges[T.e0];
		if (E0.v0 == vertex_index || E0.v1 == vertex_index)
			adjacency.push_back(T.e0);
		
		const EdgeGPU & E1 = m_base_edges[T.e1];
		if (E1.v0 == vertex_index || E1.v1 == vertex_index)
			adjacency.push_back(T.e1);
		
		const EdgeGPU & E2 = m_base_edges[T.e2];
		if (E2.v0 == vertex_index || E2.v1 == vertex_index)
			adjacency.push_back(T.e2);
	}
	for (int k = 0; k < 4; ++k)
	{
		if (vertex.faces_1[k] == -1)
			continue;
		const TriangleGPU & T = m_base_triangles[vertex.faces_1[k]];

		const EdgeGPU & E0 = m_base_edges[T.e0];
		if (E0.v0 == vertex_index || E0.v1 == vertex_index)
			adjacency.push_back(T.e0);

		const EdgeGPU & E1 = m_base_edges[T.e1];
		if (E1.v0 == vertex_index || E1.v1 == vertex_index)
			adjacency.push_back(T.e1);

		const EdgeGPU & E2 = m_base_edges[T.e2];
		if (E2.v0 == vertex_index || E2.v1 == vertex_index)
			adjacency.push_back(T.e2);
	}
}

void RenderablePlanet::bindBuffers() 
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_edge_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_vertex_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_face_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_pos_vbo);

	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 5, m_edge_counter);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 6, m_vertex_counter);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 7, m_face_counter);
}

void RenderablePlanet::renderDebug(const math::dvec3 & cameraPosition, const math::dmat4 & view, const math::dmat4 & projection)
{
	unsigned int count = 0;
	std::vector<unsigned int> indices;
	indices.reserve(3 * 200000);
	for (int i = 0; i < m_face_end; ++i)
	{
		if (m_base_triangles[i].status != 0)
			continue;

		const EdgeGPU & e0 = m_base_edges[m_base_triangles[i].e0];
		const EdgeGPU & e1 = m_base_edges[m_base_triangles[i].e1];
		const EdgeGPU & e2 = m_base_edges[m_base_triangles[i].e2];

		bool flip0 = false;
		bool flip1 = false;
		bool flip2 = false;//determine winding of edges (as they can appear in any order)
		if (e0.v0 == e1.v0 || e0.v0 == e1.v1)
			flip0 = true;
		if (e1.v0 == e2.v0 || e1.v0 == e2.v1)
			flip1 = true;
		if (e2.v0 == e0.v0 || e2.v0 == e0.v1)
			flip2 = true;

		const uint v0 = uint(flip0 ? e0.v1 : e0.v0);
		const uint v1 = uint(flip1 ? e1.v1 : e1.v0);
		const uint v2 = uint(flip2 ? e2.v1 : e2.v0);

		indices.push_back(v0);
		indices.push_back(v1);
		indices.push_back(v2);

		count++;
	}

	std::cout << count << "  ";

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, 3 * count * sizeof(unsigned int), (const void*)indices.data());
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


	// --- render ---
	GLuint prog = m_render_test->getProgramID();
	glUseProgram(prog);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_vertex_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_face_buffer);

	glUniformMatrix4dv(glGetUniformLocation(prog, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4dv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniform1d(glGetUniformLocation(prog, "renderScale"), RENDER_SCALE);

	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, count * 3, GL_UNSIGNED_INT, (void*)NULL);
	glBindVertexArray(0);

	glUseProgram(0);
}

bool RenderablePlanet::loadTextures()
{
	QString mountaingrasstex_filename("../assets/img/grassland-seamless-v1.png");
	QImage mountaingrassImg;
	if (!mountaingrassImg.load(mountaingrasstex_filename))
	{
		std::cout << "ERROR : could not load texture " << mountaingrasstex_filename.toStdString() << std::endl;
		return false;
	}
	int w = mountaingrassImg.width();
	int h = mountaingrassImg.height();
	unsigned char * data = new unsigned char[3 * w*h];
	for (int Y = 0; Y < h; ++Y)
		for (int X = 0; X < w; ++X)
		{
			QRgb color = mountaingrassImg.pixel(X, Y);
			data[3 * (Y*w + X) + 0] = qRed(color);
			data[3 * (Y*w + X) + 1] = qGreen(color);
			data[3 * (Y*w + X) + 2] = qBlue(color);
		}
	glGenTextures(1, &m_mountainGrassTexture);
	glBindTexture(GL_TEXTURE_2D, m_mountainGrassTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, w, h);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);
	delete[] data;

	QString rocktex_filename("../assets/img/rock.png");
	QImage rockImg;
	if (!rockImg.load(rocktex_filename))
	{
		std::cout << "ERROR : could not load texture " << rocktex_filename.toStdString() << std::endl;
		return false;
	}
	w = rockImg.width();
	h = rockImg.height();
	data = new unsigned char[3 * w*h];
	for (int Y = 0; Y < h; ++Y)
		for (int X = 0; X < w; ++X)
		{
			QRgb color = rockImg.pixel(X, Y);
			data[3 * (Y*w + X) + 0] = qRed(color);
			data[3 * (Y*w + X) + 1] = qGreen(color);
			data[3 * (Y*w + X) + 2] = qBlue(color);
		}
	
	glGenTextures(1, &m_rockTexture);
	glBindTexture(GL_TEXTURE_2D, m_rockTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, w, h);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	delete[] data;

	QString destex_filename("../assets/img/desert2.png");
	QImage desImg;
	if (!desImg.load(destex_filename))
	{
		std::cout << "ERROR : could not load texture " << destex_filename.toStdString() << std::endl;
		return false;
	}
	w = desImg.width();
	h = desImg.height();
	data = new unsigned char[3 * w*h];
	for (int Y = 0; Y < h; ++Y)
		for (int X = 0; X < w; ++X)
		{
			QRgb color = desImg.pixel(X, Y);
			data[3 * (Y*w + X) + 0] = qRed(color);
			data[3 * (Y*w + X) + 1] = qGreen(color);
			data[3 * (Y*w + X) + 2] = qBlue(color);
		}
	glGenTextures(1, &m_desertTexture);
	glBindTexture(GL_TEXTURE_2D, m_desertTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, w, h);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);
	delete[] data;

	QString stex_filename("../assets/img/snow.png");
	QImage sImg;
	if (!sImg.load(stex_filename))
	{
		std::cout << "ERROR : could not load texture " << stex_filename.toStdString() << std::endl;
		return false;
	}
	w = sImg.width();
	h = sImg.height();
	data = new unsigned char[3 * w*h];
	for (int Y = 0; Y < h; ++Y)
		for (int X = 0; X < w; ++X)
		{
			QRgb color = sImg.pixel(X, Y);
			data[3 * (Y*w + X) + 0] = qRed(color);
			data[3 * (Y*w + X) + 1] = qGreen(color);
			data[3 * (Y*w + X) + 2] = qBlue(color);
		}
	glGenTextures(1, &m_snowTexture);
	glBindTexture(GL_TEXTURE_2D, m_snowTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, w, h);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

bool RenderablePlanet::initGL(std::ostream & shader_log)
{
	// -- textures --
	if (!loadTextures() || !initNoiseTexture() || !initProfilesTexture())
		return false;

	// -- shaders --
	m_compute_edgesplit = new Shader("../assets/shaders/edgeSplit.comp", shader_log);
	m_compute_edgesplit_simple = new Shader("../assets/shaders/edgeSplit_simple.comp", shader_log);
	m_compute_ghostmarking = new Shader("../assets/shaders/ghostMarking.comp", shader_log);
	m_compute_ghostsplit = new Shader("../assets/shaders/ghostSplit.comp", shader_log);
	m_compute_facesplit_riverbranching = new Shader("../assets/shaders/faceSplit_river.comp", shader_log);
	m_compute_facesplit_erosion = new Shader("../assets/shaders/faceSplit_erosion.comp", shader_log);
	m_compute_facesplit_simple = new Shader("../assets/shaders/facesplit_simple.comp", shader_log);
	m_compute_profiles = new Shader("../assets/shaders/profiles.comp", shader_log);
	m_compute_postprocess1 = new Shader("../assets/shaders/postprocess1.comp", shader_log);
	m_compute_postprocess2 = new Shader("../assets/shaders/postprocess2.comp", shader_log);
	m_compute_water_0 = new Shader("../assets/shaders/water0.comp", shader_log);
	m_compute_water_1 = new Shader("../assets/shaders/water1.comp", shader_log);
	m_compute_waterghosts = new Shader("../assets/shaders/waterGhosts.comp", shader_log);
	m_compute_ibo = new Shader("../assets/shaders/ibo.comp", shader_log);
	m_render_test = new Shader("../assets/shaders/render_test.vert", "../assets/shaders/render_test.frag", shader_log);
	m_geometry_terrain_pass = new Shader("../assets/shaders/geometry_terrain.vert", "../assets/shaders/geometry_terrain.frag", shader_log);
	m_geometry_water_pass = new Shader("../assets/shaders/geometry_water.vert", "../assets/shaders/geometry_water.frag", shader_log);
	m_render_deferred_final = new Shader("../assets/shaders/final.vert", "../assets/shaders/final.frag", shader_log);
	if (!m_compute_edgesplit->init() || !m_compute_edgesplit_simple->init() 
		|| !m_compute_ghostmarking->init()
		|| !m_compute_ghostsplit->init() 
		|| !m_compute_facesplit_riverbranching->init() || !m_compute_facesplit_erosion->init() || !m_compute_facesplit_simple->init() 
		|| !m_compute_profiles->init() 
		|| !m_compute_postprocess1->init() || !m_compute_postprocess2->init()
		|| !m_compute_water_0->init() || !m_compute_water_1->init()
		|| !m_compute_ibo->init()
		|| !m_render_test->init()
		|| !m_geometry_terrain_pass->init() || !m_geometry_water_pass->init() || !m_render_deferred_final->init()
		)
	{
		std::cout << "ERROR COMPILING SHADER(s)." << std::endl;
		return false;
	}

	// -- terrain --
	glGenBuffers(1, &m_pos_vbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pos_vbo);
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(VertexAttributesGPU) * PLANET_MAX_TRIANGLES, NULL, GL_DYNAMIC_STORAGE_BIT);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_base_vattrib.size() * sizeof(VertexAttributesGPU), (const void*)m_base_vattrib.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	glGenBuffers(1, &m_ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, (3) * sizeof(unsigned int) * PLANET_MAX_TRIANGLES, NULL, GL_DYNAMIC_STORAGE_BIT);
	glBindBuffer(GL_ARRAY_BUFFER, m_pos_vbo);
	const GLuint attribsize = 2 * 4 * sizeof(double) + 4 * 4 * sizeof(float);
	glVertexAttribLPointer(0, 4, GL_DOUBLE, attribsize, (void*)0);//position (xyz) + altitude(w)
	glEnableVertexAttribArray(0);
	glVertexAttribLPointer(1, 4, GL_DOUBLE, attribsize, (void*)(4 * sizeof(double)));// river data
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, attribsize, (void*)(2*4 * sizeof(double) + 0 * sizeof(float)));// flow
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, attribsize, (void*)(2*4 * sizeof(double) + 1 * 4 * sizeof(float)));// misc1
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, attribsize, (void*)(2*4 * sizeof(double) + 2 * 4 * sizeof(float)));// misc2
	glEnableVertexAttribArray(4);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// -- water --
	glGenBuffers(1, &m_water_vbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_water_vbo);
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(WaterVertexAttributesGPU) * PLANET_MAX_TRIANGLES / 2, NULL, GL_DYNAMIC_STORAGE_BIT);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glGenVertexArrays(1, &m_water_vao);
	glBindVertexArray(m_water_vao);
	glGenBuffers(1, &m_water_ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_water_ibo);
	glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, (3) * sizeof(unsigned int) * PLANET_MAX_TRIANGLES / 2, NULL, GL_DYNAMIC_STORAGE_BIT);
	glBindBuffer(GL_ARRAY_BUFFER, m_water_vbo);
	const GLuint wattribsize = 4 * sizeof(double) + 2 * 4 * sizeof(float) + 8*sizeof(int);
	glVertexAttribLPointer(0, 4, GL_DOUBLE, wattribsize, (void*)0);//position (xyz) + average local water altitude(w)
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, wattribsize, (void*)(4 * sizeof(double)));// xyz : water normal, w : ?
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, wattribsize, (void*)(4 * sizeof(double) + 4 * sizeof(float)));// water flow (xyz)
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// -- tessellation compute --
	glGenBuffers(1, &m_edge_counter);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_edge_counter);
	//glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint) * 1, NULL, GL_DYNAMIC_COPY);
	glBufferStorage(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint) * 1, NULL, GL_DYNAMIC_STORAGE_BIT);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	glGenBuffers(1, &m_vertex_counter);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_vertex_counter);
	glBufferStorage(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint) * 1, NULL, GL_DYNAMIC_STORAGE_BIT);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	glGenBuffers(1, &m_face_counter);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_face_counter);
	glBufferStorage(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint) * 1, NULL, GL_DYNAMIC_STORAGE_BIT);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	glGenBuffers(1, &m_ibo_counter);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_ibo_counter);
	glBufferStorage(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint) * 1, NULL, GL_DYNAMIC_STORAGE_BIT);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	glGenBuffers(1, &m_water_ibo_counter);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_water_ibo_counter);
	glBufferStorage(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint) * 1, NULL, GL_DYNAMIC_STORAGE_BIT);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

	glGenBuffers(1, &m_edge_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_edge_buffer);
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(EdgeGPU) * PLANET_MAX_TRIANGLES, 0, GL_DYNAMIC_STORAGE_BIT);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(EdgeGPU) * m_base_edges.size(), (const void*)m_base_edges.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glGenBuffers(1, &m_vertex_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_vertex_buffer);
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(VertexGPU) * PLANET_MAX_TRIANGLES, 0, GL_DYNAMIC_STORAGE_BIT);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(VertexGPU) * m_base_vertices.size(), (const void*)m_base_vertices.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glGenBuffers(1, &m_face_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_face_buffer);
	//glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(TriangleGPU) * PLANET_MAX_TRIANGLES, 0, GL_DYNAMIC_COPY);
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(TriangleGPU) * PLANET_MAX_TRIANGLES, 0, GL_DYNAMIC_STORAGE_BIT);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(TriangleGPU) * m_base_triangles.size(), (const void*)m_base_triangles.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

	m_edge_start = 0;
	m_edge_end = m_base_edges.size();

	m_vertex_start = 0;
	m_vertex_end = m_base_vertices.size();

	m_face_start = 0;
	m_face_end = m_base_triangles.size();

	// -- (Deferred Rendering) Render Targets --
	if (!buildRenderTargets())
		return false;

	return true;
}

void RenderablePlanet::impl_reload_shaders()
{
	m_compute_edgesplit->destroy();
	delete m_compute_edgesplit;
	m_compute_edgesplit_simple->destroy();
	delete m_compute_edgesplit_simple;

	m_compute_ghostmarking->destroy();
	delete m_compute_ghostmarking;
	m_compute_ghostsplit->destroy();
	delete m_compute_ghostsplit;
	
	m_compute_facesplit_riverbranching->destroy();
	delete m_compute_facesplit_riverbranching;
	m_compute_facesplit_erosion->destroy();
	delete m_compute_facesplit_erosion;
	m_compute_facesplit_simple->destroy();
	delete m_compute_facesplit_simple;
	
	m_compute_profiles->destroy();
	delete m_compute_profiles;
	
	m_compute_postprocess1->destroy();
	delete m_compute_postprocess1;
	m_compute_postprocess2->destroy();
	delete m_compute_postprocess2;
	m_compute_water_0->destroy();
	delete m_compute_water_0;
	m_compute_water_1->destroy();
	delete m_compute_water_1;

	m_compute_ibo->destroy();
	delete m_compute_ibo;

	m_render_test->destroy();
	delete m_render_test;

	if (m_geometry_terrain_pass != nullptr)
	{
		m_geometry_terrain_pass->destroy();
		delete m_geometry_terrain_pass;
	}
	if (m_geometry_water_pass != nullptr)
	{
		m_geometry_water_pass->destroy();
		delete m_geometry_water_pass;
	}
	if (m_render_deferred_final != nullptr)
	{
		m_render_deferred_final->destroy();
		delete m_render_deferred_final;
	}
	
	std::ostream & shader_log = std::cout;
	m_compute_edgesplit = new Shader("../assets/shaders/edgeSplit.comp", shader_log);
	m_compute_edgesplit_simple = new Shader("../assets/shaders/edgeSplit_simple.comp", shader_log);
	m_compute_ghostmarking = new Shader("../assets/shaders/ghostMarking.comp", shader_log);
	m_compute_ghostsplit = new Shader("../assets/shaders/ghostSplit.comp", shader_log);
	m_compute_facesplit_riverbranching = new Shader("../assets/shaders/faceSplit_river.comp", shader_log);
	m_compute_facesplit_erosion = new Shader("../assets/shaders/faceSplit_erosion.comp", shader_log);
	m_compute_facesplit_simple = new Shader("../assets/shaders/facesplit_simple.comp", shader_log);
	m_compute_profiles = new Shader("../assets/shaders/profiles.comp", shader_log);
	m_compute_postprocess1 = new Shader("../assets/shaders/postprocess1.comp", shader_log);
	m_compute_postprocess2 = new Shader("../assets/shaders/postprocess2.comp", shader_log);
	m_compute_water_0 = new Shader("../assets/shaders/water0.comp", shader_log);
	m_compute_water_1 = new Shader("../assets/shaders/water1.comp", shader_log);
	m_compute_ibo = new Shader("../assets/shaders/ibo.comp", shader_log);
	m_render_test = new Shader("../assets/shaders/render_test.vert", "../assets/shaders/render_test.frag", shader_log);
	m_geometry_terrain_pass = new Shader("../assets/shaders/geometry_terrain.vert", "../assets/shaders/geometry_terrain.frag", shader_log);
	m_geometry_water_pass = new Shader("../assets/shaders/geometry_water.vert", "../assets/shaders/geometry_water.frag", shader_log);
	m_render_deferred_final = new Shader("../assets/shaders/final.vert", "../assets/shaders/final.frag", shader_log);
	if (!m_compute_edgesplit->init() || !m_compute_edgesplit_simple->init() 
		|| !m_compute_ghostmarking->init()
		|| !m_compute_ghostsplit->init() 
		|| !m_compute_facesplit_riverbranching->init() || !m_compute_facesplit_erosion->init() || !m_compute_facesplit_simple->init() 
		|| !m_compute_profiles->init() 
		|| !m_compute_postprocess1->init() || !m_compute_postprocess2->init()
		|| !m_compute_water_0->init() || !m_compute_water_1->init()
		|| !m_compute_ibo->init()
		|| !m_render_test->init()
		|| !m_geometry_terrain_pass->init() || !m_geometry_water_pass->init() || !m_render_deferred_final->init()
		)
	{
		std::cout << "ERROR COMPILING SHADER(s)." << std::endl;
		return;
	}

	std::cout << "SHADERS RELOADED SUCCESSFULLY." << std::endl;
}

bool RenderablePlanet::buildRenderTargets()
{
	if (m_viewwidth <= 0 || m_viewheight <= 0)
		return true;

	glDeleteTextures(1, &m_geometry_buffer_0);
	glGenTextures(1, &m_geometry_buffer_0); // (position)
	glBindTexture(GL_TEXTURE_2D, m_geometry_buffer_0);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, m_viewwidth, m_viewheight);

	glDeleteTextures(1, &m_geometry_buffer_1);
	glGenTextures(1, &m_geometry_buffer_1);// (normal)
	glBindTexture(GL_TEXTURE_2D, m_geometry_buffer_1);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, m_viewwidth, m_viewheight);

	glDeleteTextures(1, &m_geometry_buffer_2);
	glGenTextures(1, &m_geometry_buffer_2);// (TERRAIN INFO)
	glBindTexture(GL_TEXTURE_2D, m_geometry_buffer_2);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, m_viewwidth, m_viewheight);

	glDeleteTextures(1, &m_water_geometry_buffer_0);
	glGenTextures(1, &m_water_geometry_buffer_0); // (position)
	glBindTexture(GL_TEXTURE_2D, m_water_geometry_buffer_0);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, m_viewwidth, m_viewheight);

	glDeleteTextures(1, &m_water_geometry_buffer_1);
	glGenTextures(1, &m_water_geometry_buffer_1);// (normal)
	glBindTexture(GL_TEXTURE_2D, m_water_geometry_buffer_1);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, m_viewwidth, m_viewheight);

	glDeleteRenderbuffers(1, &m_depth_buffer_terrain);
	glGenRenderbuffers(1, &m_depth_buffer_terrain);
	glBindRenderbuffer(GL_RENDERBUFFER, m_depth_buffer_terrain);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_viewwidth, m_viewheight);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glDeleteRenderbuffers(1, &m_depth_buffer_water);
	glGenRenderbuffers(1, &m_depth_buffer_water);
	glBindRenderbuffer(GL_RENDERBUFFER, m_depth_buffer_water);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_viewwidth, m_viewheight);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	GLenum DrawBuffers[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDeleteFramebuffers(1, &m_framebuffer_terrain);
	glGenFramebuffers(1, &m_framebuffer_terrain);
	glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer_terrain);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_geometry_buffer_0, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_geometry_buffer_1, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_geometry_buffer_2, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth_buffer_terrain);
	glDrawBuffers(3, DrawBuffers);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "Deferred (terrain) framebuffer incomplete!\n" << std::endl;
		return false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &m_framebuffer_water);
	glGenFramebuffers(1, &m_framebuffer_water);
	glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer_water);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_water_geometry_buffer_0, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_water_geometry_buffer_1, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth_buffer_water);
	glDrawBuffers(2, DrawBuffers);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "Deferred (water) framebuffer incomplete!\n" << std::endl;
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, m_default_fbo);

	// Timer queries:
	glGenQueries(1, &m_timequery);

	return true;
}

bool RenderablePlanet::initNoiseTexture()
{
	unsigned char * noise3d = nullptr;
#ifdef LOAD_NOISE_TEXTURE
	std::ifstream datain("../assets/noise/noise3d.data", std::ifstream::in | std::ifstream::binary);
	if (!datain.good())
	{
		std::cout << "ERROR - failed opening ../assets/noise/noise3d.data" << std::endl;
		return false;
	}
	int NOISE_SIZE;
	datain.read((char*)&NOISE_SIZE, sizeof(int));
	noise3d = new unsigned char[4 * NOISE_SIZE*NOISE_SIZE*NOISE_SIZE];
	datain.read((char*)noise3d, 4* NOISE_SIZE*NOISE_SIZE*NOISE_SIZE);
	datain.close();
#else
	const int NOISE_SIZE = 256;
	const double sized = 1.0 / (double)NOISE_SIZE;
	const math::dvec3 offset(37.7, -11.1, 68.6);
	noise3d = new unsigned char[4 * NOISE_SIZE * NOISE_SIZE * NOISE_SIZE];
	for (int Z = 0; Z < NOISE_SIZE; ++Z)
		for (int Y = 0; Y < NOISE_SIZE; ++Y)
			for (int X = 0; X < NOISE_SIZE; ++X)
			{
				math::dvec3 p((double)X, (double)Y, (double)Z);
				p *= sized;
				double n0 = tool::fractalSimplex3D(8, 0.52, 2.02, 11.0, p + offset);//for terrain effects
				double n1 = 0.68 * tool::fractalRidged3D(5, 0.5, 2.02, 5.0, p + offset) + 0.32 * tool::fractalTurbulence3D(4, 0.52, 2.02, 5.0, p + 2.0*offset);//for water waves

				noise3d[4 * (NOISE_SIZE * NOISE_SIZE * Z + NOISE_SIZE * Y + X) + 0] = (unsigned char)std::floor(n0 * 255.0);
				noise3d[4 * (NOISE_SIZE * NOISE_SIZE * Z + NOISE_SIZE * Y + X) + 1] = (unsigned char)std::floor(n1 * 255.0);
				noise3d[4 * (NOISE_SIZE * NOISE_SIZE * Z + NOISE_SIZE * Y + X) + 2] = 0;
				noise3d[4 * (NOISE_SIZE * NOISE_SIZE * Z + NOISE_SIZE * Y + X) + 3] = 0;
			}

	std::ofstream dataout("../assets/noise/noise3d.data", std::ofstream::out | std::ofstream::binary);
	if (!dataout.good())
	{
		std::cout << "ERROR - failed writing to ../assets/noise/noise3d.data" << std::endl;
		return false;
	}
	dataout.write((char*)&NOISE_SIZE, sizeof(int));
	dataout.write((char*)noise3d, 4 * NOISE_SIZE * NOISE_SIZE * NOISE_SIZE);
	dataout.close();
#endif

	glGenTextures(1, &m_noiseTexture3D);
	glBindTexture(GL_TEXTURE_3D, m_noiseTexture3D);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, NOISE_SIZE, NOISE_SIZE, NOISE_SIZE);
	glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, NOISE_SIZE, NOISE_SIZE, NOISE_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, noise3d);
	glBindTexture(GL_TEXTURE_3D, 0);

	delete[] noise3d;
	return true;
}

bool RenderablePlanet::initProfilesTexture()
{
	const int TEXTURE_PROFILES_LENGTH = 4096 * 4;
	const int TEXTURE_PROFILES_NUM_PROFILES = 4;

	float * data = new float[TEXTURE_PROFILES_NUM_PROFILES * TEXTURE_PROFILES_LENGTH];
	
	/*for (int i = 0; i < TEXTURE_PROFILES_LENGTH; ++i)
	{
		float x = (float)i / (float)(TEXTURE_PROFILES_LENGTH - 1);
		float noise = (float)tool::fractalSimplex2D(6, 0.5, 2.02, 8.0, math::dvec2((double)x + 11.3, 13.7));
		data[0 * TEXTURE_PROFILES_LENGTH + i] = math::clamp(x + 0.08f*noise, 0.0f, 1.0f);
	}
	// profile 1 : linear 
	for (int i = 0; i < TEXTURE_PROFILES_LENGTH; ++i)
	{
		float x = (float)i / (float)(TEXTURE_PROFILES_LENGTH - 1);
		float noise = (float)tool::fractalSimplex2D(6, 0.5, 2.02, 8.0, math::dvec2((double)x + 18.3, -13.7));
		data[1 * TEXTURE_PROFILES_LENGTH + i] = math::clamp(x + 0.01f*noise, 0.0f, 1.0f);
	}*/

	// profile 0 : linear with plateau
	for (int i = 0; i < TEXTURE_PROFILES_LENGTH; ++i)
	{
		const float x = (float)i / (float)(TEXTURE_PROFILES_LENGTH - 1);
		const float noise = -1.0f + 2.0f*(float)tool::fractalSimplex2D(9, 0.5, 2.02, 4.0, math::dvec2((double)x + 18.3, -13.7));
		const float noisesmoothing = math::smoothstep(0.0f, 0.06f, x);
		data[0 * TEXTURE_PROFILES_LENGTH + i] = math::clamp(1.75f *x + 0.2f * noise * noisesmoothing, 0.0f, 1.0f);
	}
	// profile 1 : linear 
	for (int i = 0; i < TEXTURE_PROFILES_LENGTH; ++i)
	{
		const float x = (float)i / (float)(TEXTURE_PROFILES_LENGTH - 1);
		const float noise = -1.0f + 2.0f*(float)tool::fractalSimplex2D(9, 0.5, 2.02, 3.0, math::dvec2((double)x + 11.3, 13.7));
		const float noisesmoothing = math::smoothstep(0.0f, 0.06f, x);
		data[1 * TEXTURE_PROFILES_LENGTH + i] = math::clamp(x + 0.2f * noise * noisesmoothing, 0.0f, 1.0f);
	}
	// profile 2 : cubic
	for (int i = 0; i < TEXTURE_PROFILES_LENGTH; ++i)
	{
		const float x = (float)i / (float)(TEXTURE_PROFILES_LENGTH - 1);
		float v = x * x*x;
		const float noise = -1.0f + 2.0f*(float)tool::fractalSimplex2D(9, 0.5, 2.02, 3.7, math::dvec2((double)x + 5.3, 11.7));
		const float noisesmoothing = math::smoothstep(0.0f, 0.06f, x);
		data[2 * TEXTURE_PROFILES_LENGTH + i] = math::clamp(v + 0.2f*noise *noisesmoothing, 0.0f, 1.0f);
	}
	// profile 3 : smoothstep
	for (int i = 0; i < TEXTURE_PROFILES_LENGTH; ++i)
	{
		const float x = (float)i / (float)(TEXTURE_PROFILES_LENGTH - 1);
		float v = math::smoothstep(0.0f, 1.0f, x);
		const float noise = -1.0f + 2.0f*(float)tool::fractalSimplex2D(9, 0.5, 2.02, 3.5, math::dvec2((double)x + 22.8, 17.7));
		const float noisesmoothing = math::smoothstep(0.0f, 0.06f, x);
		data[3 * TEXTURE_PROFILES_LENGTH + i] = math::clamp(v + 0.2f*noise *noisesmoothing, 0.0f, 1.0f);
	}
	

	glGenTextures(1, &m_profiles_texture);
	glBindTexture(GL_TEXTURE_2D, m_profiles_texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);//GL_LINEAR_MIPMAP_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);	
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, TEXTURE_PROFILES_LENGTH, TEXTURE_PROFILES_NUM_PROFILES);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TEXTURE_PROFILES_LENGTH, TEXTURE_PROFILES_NUM_PROFILES, GL_RED, GL_FLOAT, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	delete[] data;

	return true;
}
