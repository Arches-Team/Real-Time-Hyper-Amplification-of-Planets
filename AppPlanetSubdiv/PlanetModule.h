#pragma once

#define VIDEO_DURATION_SECONDS			(60.0 * 2.5)


#include "Common.h"
#include "RenderablePlanet.h"
#include "PlanetData.h"

//#include "TectonicsCommon.h"
//#include "CloudscapeManager.h"
//#include "DEMManager.h"
#include "PlanetVideoPath.h"

#include "appmodule.h"
#include "glversion.h"
#include "shader.h"
#include "OrbitCamera.h"
#include "FreeFlyCamera.h"

#include "tool.h"

#include <chrono>
#include <random>
#include <iostream>
#include <string>

#include <qimage.h>



enum class PlanetModuleState
{
	NONE,
	LOAD_PLANET,
	LOAD_EARTH,
	RENDER_PLANET
};



void ModuleGLMessageCallback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
	;


class PlanetModule : public AppModule, protected Q_OPENGL_FUNCS
{
	Q_OBJECT

public:
	PlanetModule(GLWidget * host)
		: AppModule(host)
		, m_shader_logstream()		
	{}
	virtual ~PlanetModule() { cleanup(); }
	
	virtual void init() Q_DECL_OVERRIDE;
	virtual void render() Q_DECL_OVERRIDE;
	virtual void onPostRender() Q_DECL_OVERRIDE;
	virtual void onResize(int w, int h) Q_DECL_OVERRIDE;
	virtual void cleanup() Q_DECL_OVERRIDE;

	virtual void onMousePressed(int x, int y, bool leftButton, bool rightButton) Q_DECL_OVERRIDE;
	virtual void onMouseMoved(int dx, int dy, bool leftButton, bool rightButton) Q_DECL_OVERRIDE;
	virtual void onMouseWheel(bool forward) Q_DECL_OVERRIDE;
	virtual bool onKeyPress(QKeyEvent * event) Q_DECL_OVERRIDE;
	virtual bool onKeyRelease(QKeyEvent * event) Q_DECL_OVERRIDE;

	void loadPlanet(const std::string & filename);
	void loadEarth();

	void setCameraModeOrbit(bool orbit);
	void loadFreeflyCamera() { m_load_freefly_camera = true; }
	void saveFreeflyCamera() const;
	inline void setSunDirection(const glm::dvec3 & dir)
	{
		m_sun_direction = dir;
		if (m_planet != nullptr)
			m_planet->setSunDirection(dir);
	}
	void exportVideo() { m_render_planet_video = true; }
	
	inline double getCurrentAltitude() const 
	{
		if (m_planet != nullptr)
		{
			if (m_camera_mode_freefly)
				return math::length(m_camera_freefly->getPosition()) * RENDER_SCALE - (m_planet_data->radiusKm + m_planet_data->seaLevelKm);
			else return math::length(m_camera_orbit->getPosition()) * RENDER_SCALE - (m_planet_data->radiusKm + m_planet_data->seaLevelKm);
		}
		return 0.0;
	}

	
private:
	void initDefaultFBO();
	void renderPlanet();
	void renderVideo();
	void releasePlanet();

	void saveScreenshot();
	void saveVideoFrame();
	void saveCameraPath();
	void loadCameraPath();

	void makeNoiseTexture3D();
	
private:	
	
	QImage m_frame_image;
	PlanetData * m_planet_data = nullptr;
	RenderablePlanet * m_planet = nullptr;
	
	tool::OrbitCameraDouble * m_camera_orbit = nullptr;
	tool::FreeFlyCameraDouble * m_camera_freefly = nullptr;
	const double m_fov = 60.0;
	const double m_nearplaneKm = 0.001;
	const double m_farplaneKm = 100000.0;
	bool m_simulation_render_clouds = false;
	bool m_camera_changed = false;
	bool m_camera_mode_freefly = true;
	bool m_load_freefly_camera = false;
	bool m_save_flyby = false;
	bool m_planet_reset = true;
	bool m_tessellate = true;
	bool m_reload_shaders = false;
	bool m_toggle_riverprimitives = false;
	bool m_option_puremidpoint = false;
	bool m_toggle_puremidpoint = false;
	int m_tessellation_framecount = 0;
	double m_freefly_speed = 1.0;
	
	std::ofstream m_shader_logstream;
	
	PlanetModuleState m_state = PlanetModuleState::NONE;

	glm::dvec3 m_sun_direction = glm::dvec3(1.0, 0.0, 0.0);

	bool m_default_fbo_invalid = false;
	GLuint m_default_fbo = 0, m_default_rendertexture = 0, m_default_depthbuffer = 0;
	Shader * m_default_shader;
	
	std::chrono::high_resolution_clock::time_point m_clockStart;
	std::string m_planet_filename;

	double m_ocean_translucency = 1.0;

	bool m_blockKeyboard = false;

	bool m_take_screenshot = false;

	bool m_keyboard_S_pressed = false;
	int m_currentScreenshot = 0;
		
	// video of final rendered planet:
	bool m_render_planet_video = false;
	bool m_planet_video_start = true, m_planet_video_done = false;
	bool m_save_camera_path = false, m_load_camera_path = false;
	int m_planet_video_frame = 0;	
	double m_planet_video_total_timesecs = VIDEO_DURATION_SECONDS;
	std::vector<PlanetVideoPath*> m_planet_video_paths;
	std::vector<std::vector<tool::FreeFlyCameraDouble>> m_planet_video_cameras;	

	// noise gpu
	GLuint m_noise_texture3d = 0;

	// pixel buffer
	GLuint m_pixelbuf = 0;

	bool m_lock_step = false;
};

