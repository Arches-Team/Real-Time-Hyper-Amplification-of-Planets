#ifndef SHADER_HEADER
#define SHADER_HEADER

#include "glversion.h"
#include <iostream>
#include <string>
#include <fstream>


class Shader
{
public:
	Shader() : err_out(std::cerr) { }

	Shader(std::string computeSourceFilename, std::ostream & error_out)
		: err_out(error_out)
		, computeSourceFilename_(computeSourceFilename)
		, compute_(0), vertex_(0), geometry_(0), fragment_(0), program_(0), initOK_(false)
	{}
	Shader(std::string vertexSourceFilename, std::string fragmentSourceFilename, std::ostream & error_out)
		: err_out(error_out)
		, vertexSourceFilename_(vertexSourceFilename)
		, fragmentSourceFilename_(fragmentSourceFilename)
		, compute_(0), vertex_(0), geometry_(0), fragment_(0), program_(0), initOK_(false)
	{ }

	Shader(std::string vertexSourceFilename, std::string geometrySourceFilename, std::string fragmentSourceFilename, std::ostream & error_out)
		: err_out(error_out)
		, vertexSourceFilename_(vertexSourceFilename)
		, geometrySourceFilename_(geometrySourceFilename)
		, fragmentSourceFilename_(fragmentSourceFilename)
		, compute_(0), vertex_(0), geometry_(0), fragment_(0), program_(0), initOK_(false)
	{ }

	Shader(Shader const &shader)
		: err_out(std::cerr)
		, computeSourceFilename_(shader.computeSourceFilename_)
		, vertexSourceFilename_(shader.vertexSourceFilename_)
		, geometrySourceFilename_(shader.geometrySourceFilename_)
		, fragmentSourceFilename_(shader.fragmentSourceFilename_)
		, compute_(0), vertex_(0), geometry_(0), fragment_(0), program_(0), initOK_(shader.initOK_)
	{
		if (initOK_)
			init();
	}

	~Shader()
	{
		//destroy();
	}

	bool init();
	void destroy();
	GLuint getProgramID() const;
	Shader& operator=(Shader const &shader);


private:
	bool initShaderStage(GLuint &shader, GLenum shader_stage_type, std::string const &source);

private:
	std::ostream & err_out;
	std::string computeSourceFilename_;
	std::string vertexSourceFilename_;
	std::string geometrySourceFilename_;
	std::string fragmentSourceFilename_;
	GLuint compute_;
	GLuint vertex_;
	GLuint geometry_;
	GLuint fragment_;
	GLuint program_;
	bool initOK_;
};


#endif // SHADER_HEADER
