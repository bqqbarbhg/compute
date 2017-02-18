#include "opengl.h"
#include "shader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GLenum GlShaderTypes[ShaderTypeCount] = {
	GL_VERTEX_SHADER,
	GL_FRAGMENT_SHADER,
	GL_COMPUTE_SHADER,
};

struct Shader
{
	GLuint Shaders[ShaderTypeCount];
	GLuint Program;
};

bool InitializeShader(Shader *s, const ShaderSource *sources, uint32_t numSources)
{
	bool compileFailed = false;
	bool linkFailed = false;

	for (uint32_t i = 0; i < numSources; i++)
	{
		uint32_t typeIx = (uint32_t)sources[i].Type;

		if (s->Shaders[typeIx] != 0)
		{
			fprintf(stderr, "Duplicate shader type!\n");
			return false;
		}

		GLuint shader = glCreateShader(GlShaderTypes[typeIx]);
		if (!shader)
			return false;
		s->Shaders[typeIx] = shader;

		glShaderSource(shader, 1, (const GLchar**)&sources[i].Source, NULL);

		glCompileShader(shader);

		GLint status;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (status != GL_TRUE)
		{
			GLsizei messageLen;
			GLchar message[1024];
			glGetShaderInfoLog(shader, sizeof(message), &messageLen, message);

			fprintf(stderr, "Shader compile failed: %s\n", message);

			compileFailed = true;
		}
	}

	if (compileFailed)
		return false;

	GLuint program = glCreateProgram();
	s->Program = program;

	for (uint32_t i = 0; i < ShaderTypeCount; i++)
	{
		if (s->Shaders[i])
			glAttachShader(program, s->Shaders[i]);
	}

	{
		glLinkProgram(s->Program);

		GLint status;
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if (status != GL_TRUE)
		{
			GLsizei messageLen;
			GLchar message[1024];
			glGetProgramInfoLog(program, sizeof(message), &messageLen, message);

			fprintf(stderr, "Shader link failed: %s\n", message);

			linkFailed = true;
		}
	}

	if (linkFailed)
		return false;

	return true;
}

void DestroyShader(Shader *s)
{
	for (uint32_t i = 0; i < ShaderTypeCount; i++)
	{
		if (s->Shaders[i])
			glDeleteShader(s->Shaders[i]);
	}

	if (s->Program)
		glDeleteProgram(s->Program);

	free(s);
}

Shader *CreateShader(const ShaderSource *sources, uint32_t numSources)
{
	Shader *s = (Shader*)malloc(sizeof(Shader));
	memset(s, 0, sizeof(Shader));

	if (!InitializeShader(s, sources, numSources))
	{
		DestroyShader(s);
		s = NULL;
	}

	return s;
}

void SetShader(Shader *s)
{
	glUseProgram(s->Program);
}

