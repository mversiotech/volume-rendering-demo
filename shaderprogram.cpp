#include "shaderprogram.h"
#include "opengl.h"

#include <stdio.h>

ShaderProgram::ShaderProgram() : program(0), log(NULL)
{
}

ShaderProgram::~ShaderProgram()
{
	Destroy();
}

void ShaderProgram::Destroy()
{
	if (this->program != 0)
	{
		glDeleteProgram(this->program);
		CheckGLError();
		this->program = 0;
	}

	if (this->log != NULL)
	{
		delete[] this->log;
		this->log = NULL;
	}
}

unsigned int ShaderProgram::CompileShader(unsigned int type, const char *src)
{
	GLuint handle = glCreateShader(type);

	if (handle == 0)
	{
		CheckGLError();
		return 0;
	}

	glShaderSource(handle, 1, &src, NULL);
	CheckGLError();

	glCompileShader(handle);
	CheckGLError();

	GLint result;

	glGetShaderiv(handle, GL_COMPILE_STATUS, &result);

	if (result == GL_TRUE)
	{
		return handle;
	}

	GLint len;
	glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &len);

	if (this->log != NULL)
	{
		delete[] this->log;
	}

	this->log = new char[len + 1];
	glGetShaderInfoLog(handle, len, NULL, this->log);
	this->log[len] = 0;

	glDeleteShader(handle);

	return 0;
}

bool ShaderProgram::Build(const char *vsrc, const char *fsrc)
{
	GLuint vshader = CompileShader(GL_VERTEX_SHADER, vsrc);

	if (vshader == 0)
	{
		return false;
	}

	GLuint fshader = CompileShader(GL_FRAGMENT_SHADER, fsrc);

	if (fshader == 0)
	{
		glDeleteShader(vshader);
		return false;
	}

	GLuint prog = glCreateProgram();
	CheckGLError();

	if (prog == 0)
	{
		glDeleteShader(vshader);
		glDeleteShader(fshader);
		return false;
	}

	glAttachShader(prog, vshader);
	glAttachShader(prog, fshader);
	glLinkProgram(prog);
	CheckGLError();

	glDeleteShader(vshader);
	glDeleteShader(fshader);

	GLint result;
	glGetProgramiv(prog, GL_LINK_STATUS, &result);

	if (result == GL_TRUE)
	{
		this->program = prog;
		return true;
	}

	GLint len;
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);

	if (this->log != NULL)
	{
		delete[] this->log;
	}

	this->log = new char[len + 1];
	glGetProgramInfoLog(prog, len, NULL, this->log);
	this->log[len] = 0;

	glDeleteProgram(prog);

	return false;
}
	
const char *ShaderProgram::GetBuildLog()
{
	return this->log;
}

void ShaderProgram::Enable()
{
	if (this->program != 0)
	{
		glUseProgram(this->program);
		CheckGLError();
	}
}

void ShaderProgram::Disable()
{
	glUseProgram(0);
}

int ShaderProgram::GetUniformLocation(const char *name)
{
	int loc = glGetUniformLocation(this->program, name);

	if (loc < 0)
	{
		fprintf(stderr, "Can't locate uniform %s\n", name);
	}

	return loc;
}

int ShaderProgram::GetAttributeLocation(const char *name)
{
	int loc = glGetAttribLocation(this->program, name);

	if (loc < 0)
	{
		fprintf(stderr, "Can't locate attribute %s\n", name);
	}

	return loc;
}
