#pragma once
#ifndef Q_OPENGL_FUNCS
// If you want to change opengl version, this is the right place

#define APP_GL_VERSION_MAJOR    4
#define APP_GL_VERSION_MINOR    5

#include <QtGui/QOpenGLFunctions_4_5_Core>
#define Q_OPENGL_FUNCS QOpenGLFunctions_4_5_Core

#endif
