#include "renderer.h"
#include "opengl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unordered_map>

struct Buffer
{
	GLuint Buf;
	GLenum BindPoint;
};

GLenum GlBufferType[] =
{
	GL_ARRAY_BUFFER,
	GL_ELEMENT_ARRAY_BUFFER,
	GL_UNIFORM_BUFFER,
	GL_SHADER_STORAGE_BUFFER,
};

Buffer *CreateBuffer(BufferType type)
{
	Buffer *b = (Buffer*)malloc(sizeof(Buffer));
	glGenBuffers(1, &b->Buf);
	b->BindPoint = GlBufferType[type];
	return b;
}

Buffer *CreateStaticBuffer(BufferType type, const void *data, size_t size)
{
	Buffer *b = CreateBuffer(type);
	GLenum bp = b->BindPoint;
	glBindBuffer(bp, b->Buf);
	glBufferData(bp, size, data, GL_STATIC_DRAW);
	return b;
}

struct VertexSpec
{
	uint32_t NumElements;
	VertexElement Elements[1];
};

GLenum GlDataType[] =
{
	GL_FLOAT,
	GL_UNSIGNED_BYTE,
	GL_UNSIGNED_SHORT,
	GL_UNSIGNED_INT,
};

GLenum GlDrawType[] =
{
	GL_POINTS,
	GL_LINES,
	GL_TRIANGLES,
};

VertexSpec *CreateVertexSpec(const VertexElement *el, uint32_t count)
{
	VertexSpec *spec = (VertexSpec*)malloc(sizeof(uint32_t) + sizeof(VertexElement) * count);

	spec->NumElements = count;
	memcpy(spec->Elements, el, count * sizeof(VertexElement));

	return spec;
}

struct VertexBuffers
{
	GLuint Buffers[4];
	VertexSpec *Spec;
};

struct VBHash
{
	size_t operator()(const VertexBuffers& vb) const
	{
		return vb.Buffers[0] ^ vb.Buffers[1] ^ vb.Buffers[2] ^ vb.Buffers[3] ^ (uintptr_t)vb.Spec;
	}
};

bool operator==(const VertexBuffers& a, const VertexBuffers& b)
{
	return !memcmp(&a, &b, sizeof(VertexBuffers));
}

struct CommandBuffer
{
	std::unordered_map<VertexBuffers, GLuint, VBHash> VAOs;

	GLenum IndexType;
	GLuint IndexBuffer;
};

CommandBuffer *CreateCommandBuffer()
{
	return new CommandBuffer();
}

GLuint CreateAndBindVao(VertexSpec *spec, Buffer **buffers, uint32_t numStreams)
{
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	for (uint32_t streamI = 0; streamI < numStreams; streamI++)
	{
		glBindBuffer(GL_ARRAY_BUFFER, buffers[streamI]->Buf);

		for (uint32_t i = 0; i < spec->NumElements; i++)
		{
			const VertexElement *e = &spec->Elements[i];
			if (e->Stream != streamI)
				continue;

			glEnableVertexAttribArray(e->Index);

			glVertexAttribPointer(
					e->Index,
					e->NumComponents,
					GlDataType[e->Type],
					e->Normalized,
					e->Stride,
					(const GLvoid*)(uintptr_t)e->Offset);
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, NULL);
	return vao;
}

void SetVertexBuffers(CommandBuffer *cb, VertexSpec *spec, Buffer **buffers, uint32_t numStreams)
{
	VertexBuffers vb = { 0 };
	for (uint32_t i = 0; i < numStreams; i++)
	{
		vb.Buffers[i] = buffers[i]->Buf;
	}
	vb.Spec = spec;

	auto it = cb->VAOs.find(vb);

	if (it == cb->VAOs.end())
		cb->VAOs[vb] = CreateAndBindVao(spec, buffers, numStreams);
	else
		glBindVertexArray(it->second);
}

void SetUniformBuffer(CommandBuffer *cb, uint32_t index, Buffer *b)
{
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, b->Buf);
}

void SetIndexBuffer(CommandBuffer *cb, Buffer *b, DataType type)
{
	cb->IndexType = GlDataType[type];
	cb->IndexBuffer = b->Buf;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b->Buf);
}

void DrawIndexed(CommandBuffer *cb, DrawType type, uint32_t num, uint32_t indexOffset)
{
	glDrawElements(GlDrawType[type], num, cb->IndexType, (const GLvoid*)(uintptr_t)indexOffset);
}

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

void SetShader(CommandBuffer *cb, Shader *s)
{
	glUseProgram(s->Program);
}


