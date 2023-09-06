#ifndef SHADER_PROGRAM_H
#define SHADER_PROGRAM_H

class ShaderProgram
{
	unsigned int program;
	char *log;

	unsigned int CompileShader(unsigned int type, const char *src);

public:
	ShaderProgram();
	~ShaderProgram();

	bool Build(const char *vsrc, const char *fsrc);
	const char *GetBuildLog();

	void Destroy();

	void Enable();
	void Disable();

	int GetUniformLocation(const char *name);
	int GetAttributeLocation(const char *name);
};

#endif // SHADER_PROGRAM_H
