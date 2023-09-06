#include "opengl.h"

#include <stdio.h>

int OpenGL::initerror = 1;

bool OpenGL::Init()
{
	if (OpenGL::initerror > 0)
	{
		OpenGL::initerror = gl3wInit();
	}
	
	return OpenGL::initerror == 0;
}

bool OpenGL::VersionSupported(int major, int minor)
{
	return gl3wIsSupported(major, minor) != 0;
}

void OpenGL::CheckError(const char *file, int line)
{
	GLenum err;

	for (err = glGetError(); err != GL_NO_ERROR; err = glGetError())
	{
		fprintf(stderr, "OpenGL error at %s:%d: ", file, line);

        switch(err)
		{
        case GL_INVALID_OPERATION:
			fprintf(stderr, "Invalid operation\n");
			break;
        case GL_INVALID_ENUM:
			fprintf(stderr, "Invalid enum\n");
			break;
        case GL_INVALID_VALUE:
			fprintf(stderr, "Invalid value\n");
			break;
        case GL_OUT_OF_MEMORY:
			fprintf(stderr, "Out of memory\n");
			break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
			fprintf(stderr, "Invalid framebuffer operation\n");
			break;
		default:
			fprintf(stderr, "Unknown error (0x%x)\n", (int)err);
			break;
        }
    }
}
