#ifndef OPEN_GL_H
#define OPEN_GL_H

#include "GL/gl3w.h"

class OpenGL
{
	virtual ~OpenGL() = 0;

	static int initerror;

public:
	static bool Init();

	static bool VersionSupported(int major, int minor);

	static void CheckError(const char *file = "", int line = 0);
};

#define CheckGLError() OpenGL::CheckError(__FILE__, __LINE__)

#endif // OPEN_GL_H