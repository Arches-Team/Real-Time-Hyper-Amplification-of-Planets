#include "PlanetModule.h"

#include "glwidget.h"
#include "mainwindow.h"

#include "log.h"

#include "tool.h"

#include <qopenglframebufferobject.h>

#include <vector>
#include <unordered_map>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <chrono>
#include <random>



void ModuleGLMessageCallback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	std::fprintf(stdout, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
		(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
		type, severity, message);
}


void PlanetModule::init()
{
	m_clockStart = std::chrono::high_resolution_clock::now();	
	
	// init rendering :
	initializeOpenGLFunctions();
	glClearDepth(1.0f);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	
	m_shader_logstream = std::ofstream("../log/shader_build.log", std::ofstream::out);
	m_default_shader = new Shader("../assets/shaders/screenquad.vert", "../assets/shaders/screenquad.frag", m_shader_logstream);
	if (!m_default_shader->init()) {
		exit(EXIT_FAILURE);
	}
	
	// taille max des groupes de shaders
	GLint threads_max = 0;
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &threads_max);
	printf("-- Compute Capas --\n   threads max %d\n", threads_max);
	// dimension max des groupes de shaders
	GLint size_max[3] = {};
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &size_max[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &size_max[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &size_max[2]);
	printf("   size max %d %d %d\n", size_max[0], size_max[1], size_max[2]);
	// nombre max de groupes de shaders
	GLint groups_max[3] = {};
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &groups_max[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &groups_max[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &groups_max[2]);
	printf("   groups max %d %d %d\n", groups_max[0], groups_max[1], groups_max[2]);
	// taille de la memoire partagee
	GLint size = 0;
	glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &size);
	printf("   shared memory %dKb\n", size / 1024);
	printf("\n");
	
	initDefaultFBO();

#ifndef NDEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback((GLDEBUGPROC)ModuleGLMessageCallback, 0);
#endif

	// finish
	AppModule::init();
}

void PlanetModule::cleanup()
{
	if (!init_done)
		return;

	tool::GeometryProxy::cleanup();

	glDeleteBuffers(1, &m_pixelbuf);
	glDeleteFramebuffers(1, &m_default_fbo);
	glDeleteRenderbuffers(1, &m_default_depthbuffer);
	glDeleteTextures(1, &m_default_rendertexture);

	m_shader_logstream.close();
	m_default_shader->destroy();
	delete m_default_shader;

	releasePlanet();

	//glDeleteTextures(1, &m_noise_texture3d);
	//glDeleteBuffers(1, &m_saveimage_buf);
	
	//for (int i = 0; i < m_planet_video_paths.size(); ++i)
	//	delete m_planet_video_paths[i];
	
	AppModule::cleanup();
}

void PlanetModule::onResize(int w, int  h)
{
	AppModule::onResize(w, h);
	m_default_fbo_invalid = true;

	if (m_camera_freefly != nullptr)
		m_camera_freefly->perspective(m_fov, (double)viewportWidth / (double)viewportHeight, m_nearplaneKm / RENDER_SCALE, m_farplaneKm / RENDER_SCALE);
	if (m_camera_orbit != nullptr)
		m_camera_orbit->perspective(m_fov, (double)viewportWidth / (double)viewportHeight, m_nearplaneKm / RENDER_SCALE, m_farplaneKm / RENDER_SCALE);		
}

void PlanetModule::loadPlanet(const std::string & filename)
{
	m_planet_filename = filename;
	m_frame_image = QImage(viewportWidth, viewportHeight, QImage::Format_ARGB32);	
	m_state = PlanetModuleState::LOAD_PLANET;
	m_lock_step = false;

	MainWindow::instance()->showWaitCursor(true);
	MainWindow::instance()->showStatusMsg(QString("Loading planet..."));
}

void PlanetModule::loadEarth()
{
	m_frame_image = QImage(viewportWidth, viewportHeight, QImage::Format_ARGB32);
	m_state = PlanetModuleState::LOAD_EARTH;

	MainWindow::instance()->showWaitCursor(true);
	MainWindow::instance()->showStatusMsg(QString("Loading the earth..."));
}

void PlanetModule::setCameraModeOrbit(bool orbit)
{
	m_camera_mode_freefly = !orbit;
	if (m_camera_mode_freefly)
	{
		m_camera_freefly->setPosition(m_camera_orbit->getPosition());
		m_camera_freefly->setForwardDirection(math::normalize(math::dvec3(0.0, 0.0, 0.0) - m_camera_freefly->getPosition()));
		m_camera_freefly->setUpDirection(math::normalize(m_camera_orbit->up));
	}
	m_camera_changed = true;
}

void PlanetModule::saveFreeflyCamera() const
{
	if (m_camera_freefly == nullptr)
		return;

	std::ofstream file("../assets/camera_path/freefly_default.cam", std::ofstream::out | std::ofstream::binary);
	if (!file.good())
	{
		std::cout << "ERROR - failed writing to ../assets/camera_path/freefly_default.cam" << std::endl;
		MainWindow::instance()->showStatusMsg(QString("ERROR => cannot write to ../assets/camera_path/freefly_default.cam"));
		return;
	}

	file.write((char*)m_camera_freefly, sizeof(tool::FreeFlyCameraDouble));
	file.close();

	MainWindow::instance()->showStatusMsg(QString("Camera saved."));
}

void PlanetModule::releasePlanet()
{
	delete m_planet_data;
	m_planet_data = nullptr;

	delete m_planet;
	m_planet = nullptr;

	delete m_camera_freefly;
	delete m_camera_orbit;
	m_camera_freefly = nullptr;
	m_camera_orbit = nullptr;
}


void PlanetModule::render()
{
	if (m_default_fbo_invalid)
		initDefaultFBO();

	glEnable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, m_default_fbo);
	glViewport(0, 0, viewportWidth, viewportHeight);
	//glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	

	math::dvec3 campos;
	switch (m_state)
	{
	case PlanetModuleState::NONE:
		return;

	case PlanetModuleState::LOAD_PLANET:
		
		if (!m_lock_step)
		{
			m_lock_step = true;
			releasePlanet();
			delete m_camera_orbit;
			m_camera_orbit = new tool::OrbitCameraDouble(2.0*7000.0 / RENDER_SCALE);
			m_camera_orbit->perspective(m_fov, (double)viewportWidth / (double)viewportHeight, m_nearplaneKm / RENDER_SCALE, m_farplaneKm / RENDER_SCALE);

			delete m_camera_freefly;
			m_camera_freefly = new tool::FreeFlyCameraDouble();
			m_camera_freefly->setPosition(math::dvec3(2.0*7000.0 / RENDER_SCALE, 0.0, 0.0));
			m_camera_freefly->setForwardDirection(math::dvec3(-1.0, 0.0, 0.0));
			m_camera_freefly->setUpDirection(math::dvec3(0.0, 0.0, 1.0));
			m_camera_freefly->perspective(m_fov, (double)viewportWidth / (double)viewportHeight, m_nearplaneKm / RENDER_SCALE, m_farplaneKm / RENDER_SCALE);

			m_planet_data = new PlanetData;
			if (!m_planet_data->loadFromTectonicFile(m_planet_filename))
			{
				std::cout << " ERROR : cannot load planet data" << std::endl;
				return;
			}
			m_planet = new RenderablePlanet(m_planet_data);
			if (!m_planet->init(viewportWidth, viewportHeight, m_default_fbo, m_shader_logstream))
			{
				std::cout << " ERROR : cannot init RenderablePlanet" << std::endl;
				return;
			}

			if (m_camera_mode_freefly)
				campos = m_camera_freefly->getPosition();
			else
				campos = m_camera_orbit->getPosition();
			m_planet->tessellate(campos * RENDER_SCALE, m_fov, m_nearplaneKm, m_farplaneKm, viewportWidth, viewportHeight);

			MainWindow::instance()->showWaitCursor(false);
			MainWindow::instance()->showStatusMsg(QString("Planet loaded successfully."));
			m_state = PlanetModuleState::RENDER_PLANET;

			m_lock_step = false;
		}
		break;

	case PlanetModuleState::LOAD_EARTH:

		releasePlanet();
		delete m_camera_orbit;
		m_camera_orbit = new tool::OrbitCameraDouble(2.0*7000.0 / RENDER_SCALE);
		m_camera_orbit->perspective(m_fov, (double)viewportWidth / (double)viewportHeight, m_nearplaneKm / RENDER_SCALE, m_farplaneKm / RENDER_SCALE);

		delete m_camera_freefly;
		m_camera_freefly = new tool::FreeFlyCameraDouble();
		m_camera_freefly->setPosition(math::dvec3(2.0*7000.0 / RENDER_SCALE, 0.0, 0.0));
		m_camera_freefly->setForwardDirection(math::dvec3(-1.0, 0.0, 0.0));
		m_camera_freefly->setUpDirection(math::dvec3(0.0, 0.0, 1.0));
		m_camera_freefly->perspective(m_fov, (double)viewportWidth / (double)viewportHeight, m_nearplaneKm / RENDER_SCALE, m_farplaneKm / RENDER_SCALE);

		m_planet_data = new PlanetData;
		m_planet_data->maxAltitude = 8.4;

		if (!m_planet_data->loadFromMaps(std::string("../assets/planets/maps/Earth/")))
		{
			std::cout << " ERROR : cannot load planet data from maps" << std::endl;
			return;
		}

		m_planet = new RenderablePlanet(m_planet_data);
		if (!m_planet->init(viewportWidth, viewportHeight, m_default_fbo, m_shader_logstream))
		{
			std::cout << " ERROR : cannot init RenderablePlanet" << std::endl;
			return;
		}
		
		if (m_camera_mode_freefly)
			campos = m_camera_freefly->getPosition();
		else
			campos = m_camera_orbit->getPosition();
		m_planet->tessellate(campos * RENDER_SCALE, m_fov, m_nearplaneKm, m_farplaneKm, viewportWidth, viewportHeight);

		MainWindow::instance()->showWaitCursor(false);
		MainWindow::instance()->showStatusMsg(QString("Planet loaded successfully."));
		m_state = PlanetModuleState::RENDER_PLANET;
		break;

	case PlanetModuleState::RENDER_PLANET: 
		if (m_reload_shaders)
		{
			m_planet->reloadShaders();
			m_reload_shaders = false;
		}

		if (m_render_planet_video && !m_planet_video_done)		
			renderVideo();		
		else
		{
			renderPlanet();
			if (m_save_flyby)
			{
				saveVideoFrame();
				m_planet_video_frame++;
			}
		}
		break;

	default: break;
	}		

	if (m_save_camera_path)
	{
		saveCameraPath();
		m_save_camera_path = false;
	}
	if (m_load_camera_path)
	{
		loadCameraPath();
		m_load_camera_path = false;
	}
	if (m_take_screenshot)
	{
		saveScreenshot();
		m_take_screenshot = false;		
	}

	// Render to Qt's default framebuffer...	
	QOpenGLFramebufferObject::bindDefault();
	glViewport(0, 0, viewportWidth, viewportHeight);
	glDisable(GL_DEPTH_TEST);
	glCullFace(GL_BACK);

	GLuint prog = m_default_shader->getProgramID();
	glUseProgram(prog);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_default_rendertexture);
	glUniform1i(glGetUniformLocation(prog, "blitTexture"), 0);

	glBindVertexArray(tool::GeometryProxy::get_default_quad_vao());
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glUseProgram(0);
}

void PlanetModule::renderPlanet()
{
	// - render -
	bool force_tesselate = false;

	math::dmat4 view;
	math::dmat4 projection;
	math::dvec3 campos;

	if (m_camera_mode_freefly)
	{
		if (m_load_freefly_camera)
		{
			std::ifstream file("../assets/camera_path/freefly_default.cam", std::ifstream::in | std::ifstream::binary);
			if (!file.good())
			{
				std::cout << "ERROR - failed opening ../assets/camera_path/freefly_default.cam" << std::endl;
				MainWindow::instance()->showStatusMsg(QString("ERROR => cannot open ../assets/camera_path/freefly_default.cam"));
				return;
			}

			file.read((char*)m_camera_freefly, sizeof(tool::FreeFlyCameraDouble));
			file.close();

			m_load_freefly_camera = false;

			m_camera_freefly->perspective(m_fov, (double)viewportWidth / (double)viewportHeight, m_nearplaneKm / RENDER_SCALE, m_farplaneKm / RENDER_SCALE);
			// Tessellate:
			force_tesselate = true;
		}

		view = m_camera_freefly->getViewMatrix();
		projection = m_camera_freefly->projection;
		campos = m_camera_freefly->getPosition();

		if (force_tesselate)
		{
			m_planet->tessellate(m_camera_freefly->getPosition() * RENDER_SCALE, m_fov, m_nearplaneKm, m_farplaneKm, viewportWidth, viewportHeight);
			force_tesselate = false;
		}
	}
	else {
		view = m_camera_orbit->getViewMatrix();
		projection = m_camera_orbit->projection;
		campos = m_camera_orbit->getPosition();

		if (force_tesselate)
		{
			m_planet->tessellate(m_camera_orbit->getPosition() * RENDER_SCALE, m_fov, m_nearplaneKm, m_farplaneKm, viewportWidth, viewportHeight);
			force_tesselate = false;
		}
	}	
	
	std::chrono::duration<double> seconds = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - m_clockStart);
	m_planet->render(seconds.count(), viewportWidth, viewportHeight, campos, view, projection);	
}

void PlanetModule::renderVideo()
{
	int num_paths = m_planet_video_cameras.size();

	if (m_planet_video_start)//init: build camera path segments...
	{
		if (num_paths >= 1 && m_planet_video_cameras[m_planet_video_cameras.size() - 1].size() >= 2)
		{
			for (int i = 0; i < num_paths; ++i)
			{
				m_planet_video_paths.push_back(new PlanetVideoPath(m_planet_data->radiusKm / RENDER_SCALE));
				m_planet_video_paths[i]->setStartCamera(m_planet_video_cameras[i][0]);
				m_planet_video_paths[i]->setEndCamera(m_planet_video_cameras[i][m_planet_video_cameras[i].size() -1]);
				for (int j = 0; j < m_planet_video_cameras[i].size(); ++j)
					m_planet_video_paths[i]->addCameraPosition(m_planet_video_cameras[i][j].getPosition());
				m_planet_video_paths[i]->buildPath();
			}
		}
		else {
			m_render_planet_video = false;//(invalid path segments)
			return;
		}

		MainWindow::instance()->showWaitCursor(true);
		m_planet_video_frame = 0;
		m_planet_video_start = false;
	}

	const int FRAMES_PER_SECOND = 24;
	int total_frames = FRAMES_PER_SECOND * (int)m_planet_video_total_timesecs;
	double currentTimeSecs = (double)m_planet_video_frame / (double)FRAMES_PER_SECOND;
	
	// Compute current segment and interpolation:
	double t = (double)m_planet_video_frame / (double)(total_frames - 1);
	int segment = 0;
	for (; segment < num_paths; ++segment)
		if (t <= (double)(1+segment) / (double)(num_paths))
			break;
	double t0 = (double)segment / (double)(num_paths);
	double t1 = (double)(segment + 1) / (double)num_paths;
	t = (t - t0) / (t1 - t0);

	// Interpolate camera:
	tool::FreeFlyCameraDouble current_camera = m_planet_video_paths[segment]->getCamera(t);
	current_camera.perspective(m_fov, (double)viewportWidth / (double)viewportHeight, m_nearplaneKm / RENDER_SCALE, m_farplaneKm / RENDER_SCALE);
	//m_planet->setCameraProjection(fov, nearPlane, viewportWidth, viewportHeight);
	//m_planet->setCameraPosition(current_camera.getPosition());

	// Tessellate:
	m_planet->tessellate(current_camera.getPosition() * RENDER_SCALE, m_fov, m_nearplaneKm, m_farplaneKm, viewportWidth, viewportHeight);

	// Render:
	math::dmat4 view;
	math::dmat4 projection;
	math::dvec3 campos;
	view = current_camera.getViewMatrix();
	projection = current_camera.projection;
	campos = current_camera.getPosition();
	
	std::chrono::duration<double> seconds = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - m_clockStart);
	m_planet->render(seconds.count(), viewportWidth, viewportHeight, campos, view, projection);
	//m_planet->render(m_currentTimeSecs, viewportWidth, viewportHeight, campos, view, projection);//water animation is flawed when rendering video, somehow, but why?

	// Save:
	saveVideoFrame();
	m_planet_video_frame++;
	if (m_planet_video_frame >= total_frames)
	{
		std::exit(EXIT_SUCCESS);
	}

	char info[16];
	std::sprintf(info, "%d/%d", m_planet_video_frame, total_frames);
	QString msg = QString("Video : rendering... frame ") + QString(info);
	MainWindow::instance()->showStatusMsg(msg);
}

void PlanetModule::onPostRender()
{
	if (m_render_planet_video)
		return;

	if (m_camera_changed && m_planet != nullptr && m_tessellation_framecount > 9)
	{
		math::dvec3 campos;
		if (m_camera_mode_freefly)
			campos = m_camera_freefly->getPosition();
		else
			campos = m_camera_orbit->getPosition();

		if (m_tessellate)
			m_planet->tessellate(campos * RENDER_SCALE, m_fov, m_nearplaneKm, m_farplaneKm, viewportWidth, viewportHeight);

		m_camera_changed = false;
		m_tessellation_framecount = 0;
	}
	else if (m_camera_changed)
		m_tessellation_framecount++;
}


void PlanetModule::onMousePressed(int x, int y, bool leftButton, bool rightButton)
{
	
}

void PlanetModule::onMouseMoved(int dx, int dy, bool leftButton, bool rightButton)
{
	if (m_state == PlanetModuleState::RENDER_PLANET && leftButton)
	{			
		if (m_camera_orbit != nullptr && !m_camera_mode_freefly)
		{			
			const double altitude = m_camera_orbit->radius * RENDER_SCALE - (m_planet_data->radiusKm + m_planet_data->seaLevelKm);
			const double damping = math::smoothstep(-100.0, 2500.0, altitude);
			m_camera_orbit->orbit(-0.5 * (double)dx * damping, 0.5 * (double)dy * damping);
			m_camera_changed = true;
		}
		else if (m_camera_freefly != nullptr && m_camera_mode_freefly)
		{
			m_camera_freefly->yaw(-0.5 * (double)dx);
			m_camera_freefly->pitch(0.5 * (double)dy);
		}		
	}	
}

void PlanetModule::onMouseWheel(bool forward)
{
	if (m_camera_orbit != nullptr && !m_camera_mode_freefly)
	{
		m_camera_changed = true;

		const double altitude = m_camera_orbit->radius * RENDER_SCALE - (m_planet_data->radiusKm + m_planet_data->seaLevelKm);
		const double damping = math::smoothstep(-300.0, 2500.0, altitude);
		if (forward)
			m_camera_orbit->zoom(1.0 - 0.03 * damping);
		else m_camera_orbit->zoom(1.0 + 0.03 * damping);
	}
	else if (m_camera_freefly && m_camera_mode_freefly)
	{
		if (forward)
			m_freefly_speed = math::clamp(1.7 * m_freefly_speed, 0.0, 5.0);
		else m_freefly_speed = math::clamp(0.59 * m_freefly_speed, 0.0, 5.0);
	}
}

bool PlanetModule::onKeyPress(QKeyEvent * event)
{
	switch (event->key()) 
	{
	case Qt::Key_Left:
		//key_left = true;
		if (m_camera_freefly != nullptr && m_camera_mode_freefly) {
			m_camera_freefly->roll(-2.0);
		}
		break;
	case Qt::Key_Right:
		//key_right = true;
		if (m_camera_freefly != nullptr && m_camera_mode_freefly) {
			m_camera_freefly->roll(2.0);
		}
		break;
	case Qt::Key_Down:
		//key_down = true;
		if (m_camera_freefly != nullptr && m_camera_mode_freefly) {
			glm::dvec3 viewerPos = m_camera_freefly->getPosition();
			double abs_altitude_km = glm::length(viewerPos) * RENDER_SCALE;
			double t = math::clamp(abs_altitude_km - m_planet_data->radiusKm, 0.0, 1000.0) / 1000.0;
			double viewerSpeed = -30.0 * (0.001 + t * 1.0) / RENDER_SCALE;
			m_camera_freefly->translate(viewerSpeed * m_freefly_speed);
			m_camera_changed = true;
		}
		break;
	case Qt::Key_Up:
		//key_up = true;
		if (m_camera_freefly != nullptr && m_camera_mode_freefly) {
			glm::dvec3 viewerPos = m_camera_freefly->getPosition();
			double abs_altitude_km = glm::length(viewerPos) * RENDER_SCALE;
			double t = glm::clamp(abs_altitude_km - m_planet_data->radiusKm, 0.0, 1000.0) / 1000.0;
			double viewerSpeed = 30.0 * (0.001 + t * 1.0) / RENDER_SCALE;
			m_camera_freefly->translate(viewerSpeed * m_freefly_speed);
			m_camera_changed = true;
		}
		break;
	case Qt::Key_C:
		if (!m_blockKeyboard)
		{
			m_take_screenshot = true;
			//saveScreenshot();
			m_blockKeyboard = true;
		}
		break;
	case Qt::Key_S:
		m_keyboard_S_pressed = true;
		break;
	case Qt::Key_T:
		m_tessellate = !m_tessellate;
		break;
	case Qt::Key_F6:
		if (m_planet != nullptr)
			m_planet->toggleWireframe();
		break;
	case Qt::Key_D:
		if (m_planet != nullptr)
			m_planet->toggleDrainageColor();
		break;
	case Qt::Key_U://switch between our complete model and a pure midpoint displacement model (for comparison purposes)
		if (m_planet != nullptr)
		{
			if (!m_option_puremidpoint)
				std::cout << "Switching to Pure Mid Point Displacement modeling ... "<<std::endl;
			else std::cout << "Switching back to Full model."<<std::endl;
			m_toggle_puremidpoint = true;
		}
		break;
	case Qt::Key_W:
		if (m_planet != nullptr)
		{
			m_planet->toggleGenerateDrainage();
			m_camera_changed = true;
		}
		break;
	case Qt::Key_V:
		if (m_planet != nullptr)
		{
			m_planet->toggleGenerateRiverProfiles();
			m_camera_changed = true;
		}
		break;
	case Qt::Key_Q:
		if (m_planet != nullptr)
			m_planet->toggleProfileDistanceColor();
		break;
	case Qt::Key_F:
		if (m_planet != nullptr)
			m_planet->toggleFlowDirectionColor();
		break;
	/*case Qt::Key_R:
		if (m_planet != nullptr) {
			m_planet->toggleGPURelief();
			m_camera_changed = true;
		}
		break;*/
	case Qt::Key_E:
		if (m_planet != nullptr)
		{
			m_planet->toggleGenerateLakes();
			m_camera_changed = true;
		}
		break;
	case Qt::Key_A:
		if (m_planet != nullptr) {
			m_planet->toggleTectonicAgeColor();		
		}
		break;
	case Qt::Key_P:
		if (m_planet != nullptr) {
			m_planet->togglePlateauxColor();
		}
		break;
	case Qt::Key_G:
		if (m_planet != nullptr) {
			m_planet->toggleGhostVertexColor();
		}
		break;
	case Qt::Key_B:
		if (m_planet != nullptr) {
			m_planet->toggleBlendProfiles();
			m_camera_changed = true;
		}
		break;
	case Qt::Key_K:
		if (m_planet != nullptr) {
			m_save_camera_path = true;
		}
		break;
	case Qt::Key_L:
		if (m_planet != nullptr) {
			m_load_camera_path = true;
		}
		break;
	case Qt::Key_I:
		if (m_planet != nullptr) {
			m_planet->toggleRiverPrimitives();
		}
		break;
	case Qt::Key_F5:
		if (m_planet != nullptr) {
			m_planet->toggleSimpleShading();
		}
		break;
	case Qt::Key_F12:
		if (m_planet != nullptr) {
			m_reload_shaders = true;
			m_camera_changed = true;
		}
		break;
	case Qt::Key_F9:
		m_save_flyby = !m_save_flyby;
		m_planet_video_frame = 0;
		break;
	case Qt::Key_Plus:
		MainWindow::instance()->toggleFullscreen();
		break;
	case Qt::Key_F1://new path segment
		if (m_camera_freefly != nullptr && !m_blockKeyboard)
		{
			m_planet_video_cameras.push_back(std::vector<tool::FreeFlyCameraDouble>());
			m_planet_video_cameras[m_planet_video_cameras.size() - 1].push_back(*m_camera_freefly);
			m_blockKeyboard = true;
			QString msg = QString("Video : new path segment started. Added 1 control point...");
			MainWindow::instance()->showStatusMsg(msg);
		}
		break;
	case Qt::Key_F2://new control point along current path segment
		if (m_camera_freefly != nullptr && !m_blockKeyboard)
		{
			m_planet_video_cameras[m_planet_video_cameras.size() - 1].push_back(*m_camera_freefly);
			m_blockKeyboard = true;
			QString msg = QString("Video : new camera control point added.");
			MainWindow::instance()->showStatusMsg(msg);
		}
		break;
	case Qt::Key_F4://start exporting video frames
		exportVideo();
		break;
	default: return false;
	}
	return true;
}

bool PlanetModule::onKeyRelease(QKeyEvent * event)
{
	switch (event->key())
	{
	case Qt::Key_C:
		m_blockKeyboard = false;
		return false;
		break;
	case Qt::Key_S:
		m_keyboard_S_pressed = false;
		return false;
		break;
	case Qt::Key_F1:
		m_blockKeyboard = false;
		break;
	case Qt::Key_F2:
		m_blockKeyboard = false;
		break;
	default: return false;
	}
	return true;
}

void PlanetModule::saveScreenshot()
{
	char numbering[8];
	std::sprintf(numbering, "%.2d", m_currentScreenshot);
	QString scPath("../output/screenshots/");
	QString filename = scPath + QString("screen") + QString(numbering) + ".png";

	//unsigned char * gldata = new unsigned char[viewportWidth*viewportHeight * 3];	
	//glReadPixels(0, 0, viewportWidth, viewportHeight, GL_RGB, GL_UNSIGNED_BYTE, gldata);

	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, m_pixelbuf);	
	glReadPixels(0, 0, viewportWidth, viewportHeight, GL_RGBA, GL_UNSIGNED_BYTE, (void*)0);
	unsigned char * gldata = (unsigned char*)glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
	assert(gldata);
		
	for (int y = 0; y < viewportHeight; ++y)
		for (int x = 0; x < viewportWidth; ++x)
		{
			int glindex = (viewportHeight - 1 - y) * viewportWidth + x;//flip Y coordinate

			unsigned int pixel = 255;
			pixel = (pixel << 8) | gldata[4 * glindex + 0];//R
			pixel = (pixel << 8) | gldata[4 * glindex + 1];//G
			pixel = (pixel << 8) | gldata[4 * glindex + 2];//B

			m_frame_image.setPixel(x, y, pixel);
		}
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);

	if (!m_frame_image.save(filename, nullptr, 75)) {
		std::cout << "Error - Qt could not save screenshot " << filename.toStdString() << std::endl;
		return;
	}

	m_currentScreenshot++;
	std::cout << "Image " << filename.toStdString() << " saved successfully." << std::endl;
}

void PlanetModule::saveVideoFrame()
{
	char numbering[16];
	std::sprintf(numbering, "%.5d", m_planet_video_frame);
	QString scPath("../output/videoframes/");
	QString filename = scPath + QString("frame") + QString(numbering) + ".png";

	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, m_pixelbuf);	
	glReadPixels(0, 0, viewportWidth, viewportHeight, GL_RGBA, GL_UNSIGNED_BYTE, (void*)0);
	unsigned char * gldata = (unsigned char*)glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
	assert(gldata);
	for (int y = 0; y < viewportHeight; ++y)
		for (int x = 0; x < viewportWidth; ++x)
		{
			int glindex = (viewportHeight - 1 - y) * viewportWidth + x;//flip Y coordinate

			unsigned int pixel = 255;
			pixel = (pixel << 8) | gldata[4 * glindex + 0];//R
			pixel = (pixel << 8) | gldata[4 * glindex + 1];//G
			pixel = (pixel << 8) | gldata[4 * glindex + 2];//B

			m_frame_image.setPixel(x, y, pixel);
		}
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
		
	if (!m_frame_image.save(filename, nullptr, 75)) {
		std::cout << "Error - Qt could not save video frame " << filename.toStdString() << std::endl;
		return;
	}	
	std::cout << "Rendering: frame " << filename.toStdString() << " saved successfully." << std::endl;
}

void PlanetModule::saveCameraPath()
{
	std::ofstream file("../assets/camera_path/default.cpt", std::ofstream::out | std::ofstream::binary);
	if (!file.good())
	{
		std::cout << "ERROR - failed writing to ../assets/camera_path/default.cpt" << std::endl;
		MainWindow::instance()->showStatusMsg(QString("ERROR => cannot write to ../assets/camera_path/default.cpt"));
		return;
	}

	unsigned int num_segments = m_planet_video_cameras.size();
	file.write((char*)&num_segments, sizeof(unsigned int));
	for (unsigned int s = 0; s < num_segments; s++)
	{
		unsigned int num_cameras = m_planet_video_cameras[s].size();
		file.write((char*)&num_cameras, sizeof(unsigned int));
		for (unsigned int c = 0; c < num_cameras; ++c)
		{
			tool::FreeFlyCameraDouble & camera = m_planet_video_cameras[s][c];
			file.write((char*)&camera, sizeof(tool::FreeFlyCameraDouble));
		}
	}
	file.close();
	MainWindow::instance()->showStatusMsg(QString("Camera path saved successfully... (press 'L' to load it back)."));
}

void PlanetModule::loadCameraPath()
{
	std::ifstream file("../assets/camera_path/default.cpt", std::ifstream::in | std::ifstream::binary);
	if (!file.good())
	{
		MainWindow::instance()->showStatusMsg(QString("ERROR => cannot read ../assets/camera_path/default.cpt"));
		return;
	}

	unsigned int num_segments;
	file.read((char*)&num_segments, sizeof(unsigned int));

	m_planet_video_cameras.clear();

	unsigned int num_cameras;
	tool::FreeFlyCameraDouble camera;

	for (unsigned int s = 0; s < num_segments; ++s)
	{
		file.read((char*)&num_cameras, sizeof(unsigned int));
		m_planet_video_cameras.push_back(std::vector<tool::FreeFlyCameraDouble>());

		for (unsigned int c = 0; c < num_cameras; ++c)
		{
			file.read((char*)&camera, sizeof(tool::FreeFlyCameraDouble));
			m_planet_video_cameras[m_planet_video_cameras.size() - 1].push_back(camera);
		}
	}
	file.close();
	MainWindow::instance()->showStatusMsg(QString("Camera path loaded successfully."));
}


void PlanetModule::initDefaultFBO()
{
	glDeleteFramebuffers(1, &m_default_fbo);
	glDeleteTextures(1, &m_default_rendertexture);
	glDeleteRenderbuffers(1, &m_default_depthbuffer);

	glGenRenderbuffers(1, &m_default_depthbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, m_default_depthbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, static_cast<int>(viewportWidth), static_cast<int>(viewportHeight));
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glGenTextures(1, &m_default_rendertexture); 
	glBindTexture(GL_TEXTURE_2D, m_default_rendertexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, static_cast<int>(viewportWidth), static_cast<int>(viewportHeight));
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);	

	glGenFramebuffers(1, &m_default_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_default_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_default_rendertexture, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_default_depthbuffer);
	
	GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, DrawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "Default framebuffer for planet module is incomplete!\n" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	QOpenGLFramebufferObject::bindDefault();//Qt default framebuffer	

	//
	glDeleteBuffers(1, &m_pixelbuf);
	glGenBuffers(1, &m_pixelbuf);
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, m_pixelbuf);
	glBufferData(GL_PIXEL_PACK_BUFFER_ARB, 4 * viewportWidth * viewportHeight, NULL, GL_STATIC_READ);
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);

	m_frame_image = QImage(viewportWidth, viewportHeight, QImage::Format_ARGB32);
}


void PlanetModule::makeNoiseTexture3D()
{
	std::cout << "Precomputing noise texture ...\n"<<std::endl;
#ifndef NDEBUG
	const int NOISESIZE = 128;
#else
	const int NOISESIZE = 256;
#endif
	const double sized = (double)NOISESIZE;
	float * data = new float[NOISESIZE * NOISESIZE * NOISESIZE];

#pragma omp parallel for
	for (int k = 0; k < NOISESIZE; ++k)
		for (int j = 0; j < NOISESIZE; ++j)
			for (int i = 0; i < NOISESIZE; ++i)
			{
				glm::dvec3 p = glm::vec3((double)i, (double)j, (double)k);
				p /= sized;
				p += glm::dvec3(37.7);
				data[k * NOISESIZE * NOISESIZE + j * NOISESIZE + i] = (float) tool::fractalSimplex3D(6, 0.5, 2.02, 5.0, p);
			}

	glGenTextures(1, &m_noise_texture3d);
	glBindTexture(GL_TEXTURE_3D, m_noise_texture3d);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, NOISESIZE, NOISESIZE, NOISESIZE, 0, GL_RED, GL_FLOAT, data);
	glBindTexture(GL_TEXTURE_3D, 0);
	delete[] data;
}



/*void PlanetModule::saveSimulationFrame()
{
	char numbering[16];
	std::sprintf(numbering, "%.6d", m_simulation_frame / m_sim_record_step);
	const QString scPath("../output/frames/");
	QString frameTitle;
	switch (m_simulation_color_mode)
	{
	case SimulationColorMode::VELOCITY:
		frameTitle = QString("velocity");
		break;
	case SimulationColorMode::PRESSURE:
		frameTitle = QString("pressure");
		break;
	case SimulationColorMode::TEMPERATURE:
		frameTitle = QString("temperature");
		break;
	case SimulationColorMode::DENSITY:
		frameTitle = QString("density");
		break;		
	}
	if (m_simulation_render_tracers)
		frameTitle = QString("tracers");
	if (m_simulation_render_clouds)
		frameTitle = QString("clouds");

	QString filename = scPath + frameTitle + QString(numbering) + ".png";

	glReadBuffer(GL_COLOR_ATTACHMENT0);

	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, m_saveimage_buf);
	glReadPixels(0, 0, viewportWidth, viewportHeight, GL_RGB, GL_UNSIGNED_BYTE, (void*)0);
	unsigned char * gldata = (unsigned char*)glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
	assert(gldata);
	for (int y = 0; y < viewportHeight; ++y)
		for (int x = 0; x < viewportWidth; ++x)
		{
			int glindex = (viewportHeight - 1 - y) * viewportWidth + x;//flip Y coordinate

			unsigned int pixel = 255;
			pixel = (pixel << 8) | gldata[3 * glindex + 0];//R
			pixel = (pixel << 8) | gldata[3 * glindex + 1];//G
			pixel = (pixel << 8) | gldata[3 * glindex + 2];//B

			m_frame_image.setPixel(x, y, pixel);
		}
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);

	if (!m_frame_image.save(filename, nullptr, 75)) {
		std::cout << "Error - Qt could not save frame " << filename.toStdString() << std::endl;
		return;
	}

	QString msg = QString("Frame ") + filename + QString(" saved successfully");
	MainWindow::instance()->showStatusMsg(msg);
	//std::cout << "Frame " << filename.toStdString() << " saved successfully." << std::endl;
}*/
