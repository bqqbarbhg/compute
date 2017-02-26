#include "renderer.h"
#include "opengl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <unordered_map>

struct Buffer
{
	GLuint Buf;
	GLenum BindPoint;
	size_t Size;
};

const GLenum GlBufferType[] =
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
	b->Size = 0;
	return b;
}

Buffer *CreateStaticBuffer(BufferType type, const void *data, size_t size)
{
	Buffer *b = CreateBuffer(type);
	GLenum bp = b->BindPoint;
	glBindBuffer(bp, b->Buf);
	glBufferData(bp, size, data, GL_STATIC_DRAW);
	b->Size = size;
	return b;
}

void SetBufferData(Buffer *b, const void *data, size_t size)
{
	GLenum bp = b->BindPoint;
	glBindBuffer(bp, b->Buf);
	glBufferData(bp, size, data, GL_STATIC_DRAW);
	b->Size = size;
}

void ReserveUndefinedBuffer(Buffer *b, size_t size, bool shrink)
{
	GLenum bp = b->BindPoint;
	glBindBuffer(bp, b->Buf);

	if (shrink || size > b->Size || true)
	{
		glBufferData(bp, size, NULL, GL_STATIC_DRAW);
		b->Size = size;
	}
	else
		glInvalidateBufferData(bp);

}

void *LockBuffer(Buffer *b)
{
	if (b->Size == 0)
		return NULL;

	glBindBuffer(b->BindPoint, b->Buf);
	//return glMapBufferRange(b->BindPoint, 0, b->Size, GL_MAP_WRITE_BIT);
	return glMapBuffer(b->BindPoint, GL_WRITE_ONLY);
}

void UnlockBuffer(Buffer *b)
{
	if (b->Size == 0)
		return;

	glBindBuffer(b->BindPoint, b->Buf);
	glUnmapBuffer(b->BindPoint);
}

struct Timer
{
	GLuint Queries[4];
	uint32_t QueryIndex;
	bool Active;
};

Timer *CreateTimer()
{
	Timer *t = (Timer*)malloc(sizeof(Timer));
	glGenQueries(4, t->Queries);
	t->QueryIndex = 0;
	t->Active = false;
	return t;
}

double GetTimerMilliseconds(Timer *t)
{
	if (!t->Active)
		return 0.0;
	else
	{
		GLuint64 value;
		glGetQueryObjectui64v(t->Queries[t->QueryIndex], GL_QUERY_RESULT, &value);
		return (double)value / 1000000.0;
	}
}

void StartTimer(CommandBuffer *cb, Timer *t)
{
	glBeginQuery(GL_TIME_ELAPSED, t->Queries[t->QueryIndex]);
}

void StopTimer(CommandBuffer *cb, Timer *t)
{
	glEndQuery(GL_TIME_ELAPSED);
	t->QueryIndex++;
	if (t->QueryIndex >= 4)
	{
		t->QueryIndex = 0;
		t->Active = true;
	}
}

void CopyBufferData(CommandBuffer *cb, Buffer *dst, Buffer *src, size_t dstOffset, size_t srcOffset, size_t size)
{
	glBindBuffer(GL_COPY_WRITE_BUFFER, dst->Buf);
	glBindBuffer(GL_COPY_READ_BUFFER, src->Buf);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, srcOffset, dstOffset, size);
}

struct VertexSpec
{
	uint32_t NumElements;
	VertexElement Elements[1];
};

const GLenum GlDataType[] =
{
	GL_FLOAT,
	GL_UNSIGNED_BYTE,
	GL_UNSIGNED_SHORT,
	GL_UNSIGNED_INT,
};

const GLenum GlDrawType[] =
{
	GL_POINTS,
	GL_LINES,
	GL_TRIANGLES,
};

VertexSpec *CreateVertexSpec(const VertexElement *el, uint32_t count)
{
	VertexSpec *spec = (VertexSpec*)malloc(sizeof(VertexSpec) + sizeof(VertexElement) * count);

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

	RenderStateInfo State;
};

CommandBuffer *CreateCommandBuffer()
{
	return new CommandBuffer();
}

GLuint CreateAndBindVao(CommandBuffer *cb, VertexSpec *spec, Buffer **buffers, uint32_t numStreams)
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

			if (e->Divisor > 0)
				glVertexAttribDivisor(e->Index, e->Divisor);
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
		cb->VAOs[vb] = CreateAndBindVao(cb, spec, buffers, numStreams);
	else
		glBindVertexArray(it->second);
}

void SetUniformBuffer(CommandBuffer *cb, uint32_t index, Buffer *b)
{
	glBindBufferBase(GL_UNIFORM_BUFFER, index, b->Buf);
}

void SetStorageBuffer(CommandBuffer *cb, uint32_t index, Buffer *b)
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, b->Buf);
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

void DrawIndexedInstanced(CommandBuffer *cb, DrawType type, uint32_t numInstances, uint32_t num, uint32_t indexOffset)
{
	glDrawElementsInstanced(GlDrawType[type], num, cb->IndexType, (const GLvoid*)(uintptr_t)indexOffset, numInstances);
}

void DispatchCompute(CommandBuffer *cb, uint32_t x, uint32_t y, uint32_t z)
{
	glDispatchCompute(x, y, z);
}

const GLenum GlShaderTypes[ShaderTypeCount] = {
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

struct Sampler
{
	GLuint SamplerObject;
};

const GLenum GlFilterModeMag[] =
{
	GL_NEAREST,
	GL_LINEAR,
};

const GLenum GlFilterModeMinNearest[] =
{
	GL_NEAREST_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_NEAREST,
};

const GLenum GlFilterModeMinLinear[] =
{
	GL_NEAREST_MIPMAP_LINEAR,
	GL_LINEAR_MIPMAP_LINEAR,
};

const GLenum GlWrapMode[] =
{
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_CLAMP_TO_EDGE,
};

Sampler *CreateSampler(const SamplerInfo *si)
{
	Sampler *s = (Sampler*)malloc(sizeof(Sampler));
	glGenSamplers(1, &s->SamplerObject);

	const GLenum *mag = GlFilterModeMag;
	const GLenum *min = si->Mip == FilterNearest ? GlFilterModeMinNearest : GlFilterModeMinLinear;

	glSamplerParameteri(s->SamplerObject, GL_TEXTURE_MAG_FILTER, mag[si->Mag]);
	glSamplerParameteri(s->SamplerObject, GL_TEXTURE_MIN_FILTER, min[si->Min]);
	if (si->Anisotropy > 0)
		glSamplerParameterf(s->SamplerObject, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)si->Anisotropy);
	glSamplerParameteri(s->SamplerObject, GL_TEXTURE_WRAP_S, GlWrapMode[si->WrapU]);
	glSamplerParameteri(s->SamplerObject, GL_TEXTURE_WRAP_T, GlWrapMode[si->WrapV]);
	glSamplerParameteri(s->SamplerObject, GL_TEXTURE_WRAP_R, GlWrapMode[si->WrapW]);

	return s;
}

Sampler *CreateSamplerSimple(FilterMode min, FilterMode mag, FilterMode mip, WrapMode wrap, uint32_t anisotropy)
{
	SamplerInfo si;
	si.Min = min;
	si.Mag = mag;
	si.Mip = mip;
	si.WrapU = si.WrapV = si.WrapW = wrap;
	si.Anisotropy = anisotropy;
	return CreateSampler(&si);
}

struct Texture
{
	GLuint Tex;
	GLenum BindPoint;
	TexFormat Format;
};

const GLenum GlTextureType[] =
{
	GL_TEXTURE_1D,
	GL_TEXTURE_2D,
	GL_TEXTURE_3D,
};

Texture *CreateTexture(TextureType type)
{
	Texture *t = (Texture*)malloc(sizeof(Texture));
	glGenTextures(1, &t->Tex);
	t->BindPoint = GlTextureType[type];
	return t;
}

struct GlTexFormatPair
{
	GLenum InternalFormat, Format, Type;
	bool HasDepth, HasStencil;
};

const GlTexFormatPair GlTexFormat[] =
{
	{ GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false },
	{ GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, true, false, },
};

Texture *CreateStaticTexture2D(const void **data, uint32_t levels, uint32_t width, uint32_t height, TexFormat format)
{
	const GlTexFormatPair *fmt = &GlTexFormat[format];
	Texture *t = CreateTexture(Texture2D);
	t->Format = format;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(t->BindPoint, t->Tex);

	uint32_t w = width, h = height;
	for (uint32_t i = 0; i < levels; i++)
	{
		glTexImage2D(t->BindPoint, i, fmt->InternalFormat, w, h, 0, fmt->Format, fmt->Type, data ? data[i] : NULL);

		w /= 2;
		h /= 2;
	}

	glGenerateMipmap(t->BindPoint);

	return t;
}

void SetTexture(CommandBuffer *cb, uint32_t index, Texture *tex, Sampler *sm)
{
	glActiveTexture(GL_TEXTURE0 + index);
	glBindTexture(tex->BindPoint, tex->Tex);
	glBindSampler(index, sm->SamplerObject);
}

struct Framebuffer
{
	GLuint Buf;
	GLuint DepthRenderbuffer;
	GLenum DrawBuffers[8];
	uint32_t NumColorBuffers;
};

GLenum GetFramebufferAttachmentPoint(TexFormat format)
{
	const GlTexFormatPair *tp = &GlTexFormat[format];
	if (tp->HasStencil && tp->HasDepth)
		return GL_DEPTH_STENCIL_ATTACHMENT;
	else if (tp->HasDepth)
		return GL_DEPTH_ATTACHMENT;
	else if (tp->HasStencil)
		return GL_STENCIL_ATTACHMENT;
	else
	{
		assert(0 && "Format does not support depth or stencil");
		return 0;
	}
}

Framebuffer *CreateFramebuffer(Texture **color, uint32_t numColor, Texture *depthStencilTexture, DepthStencilCreateInfo *depthStencilCreate)
{
	Framebuffer *f = (Framebuffer*)malloc(sizeof(Framebuffer));
	f->DepthRenderbuffer = 0;
	f->NumColorBuffers = numColor;

	glGenFramebuffers(1, &f->Buf);
	glBindFramebuffer(GL_FRAMEBUFFER, f->Buf);

	glActiveTexture(GL_TEXTURE0);
	for (uint32_t i = 0; i < numColor; i++)
	{
		Texture *tex = color[i];
		glBindTexture(tex->BindPoint, tex->Tex);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, tex->Tex, 0);

		f->DrawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
	}

	if (depthStencilTexture)
	{
		GLenum atc = GetFramebufferAttachmentPoint(depthStencilTexture->Format);
		glFramebufferTexture2D(GL_FRAMEBUFFER, atc, GL_TEXTURE_2D, depthStencilTexture->Tex, 0);
	}
	else if (depthStencilCreate)
	{
		DepthStencilCreateInfo *ci = depthStencilCreate;
		glGenRenderbuffers(1, &f->DepthRenderbuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, f->DepthRenderbuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, ci->Format, ci->Width, ci->Height);

		GLenum atc = GetFramebufferAttachmentPoint(ci->Format);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, atc, GL_RENDERBUFFER, f->DepthRenderbuffer);
	}

	{
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		assert(status == GL_FRAMEBUFFER_COMPLETE);
	}

	glDrawBuffers(f->NumColorBuffers, f->DrawBuffers);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return f;
}

void SetFramebuffer(CommandBuffer *cb, Framebuffer *f)
{
	if (f)
		glBindFramebuffer(GL_FRAMEBUFFER, f->Buf);
	else
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Clear(CommandBuffer *cb, const ClearInfo *ci)
{
	uint32_t flags = 0;

	if (ci->ClearColor)
	{
		const float *c = ci->Color;
		glClearColor(c[0], c[1], c[2], c[3]);
		flags |= GL_COLOR_BUFFER_BIT;
	}

	if (ci->ClearDepth)
	{
		glClearDepth(ci->Depth);
		flags |= GL_DEPTH_BUFFER_BIT;
	}

	if (ci->ClearStencil)
	{
		glClearStencil(ci->Stencil);
		flags |= GL_STENCIL_BUFFER_BIT;
	}

	if (flags)
		glClear(flags);
}

struct RenderState
{
	RenderStateInfo Info;
};

RenderState *CreateRenderState(const RenderStateInfo *rsi)
{
	RenderState *rs = (RenderState*)malloc(sizeof(RenderState));
	rs->Info = *rsi;
	return rs;
}

void SetRenderState(CommandBuffer *cb, RenderState *r)
{
	if (r->Info.DepthTest >= RsBoolFalse && r->Info.DepthTest != cb->State.DepthTest)
	{
		if (r->Info.DepthTest == RsBoolTrue)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		cb->State.DepthTest = r->Info.DepthTest;
	}

	if (r->Info.DepthWrite >= RsBoolFalse && r->Info.DepthWrite != cb->State.DepthWrite)
	{
		glDepthMask(r->Info.DepthWrite == RsBoolTrue ? GL_TRUE : GL_FALSE);
		cb->State.DepthWrite = r->Info.DepthWrite;
	}

	if (r->Info.BlendEnable >= RsBoolFalse && r->Info.BlendEnable != cb->State.BlendEnable)
	{
		if (r->Info.BlendEnable == RsBoolTrue)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);

		cb->State.BlendEnable = r->Info.BlendEnable;
	}
}

