#pragma once

#include <stdint.h>

struct Shader;
struct Buffer;
struct CommandBuffer;
struct VertexSpec;
struct Sampler;
struct Texture;
struct Framebuffer;

enum BufferType
{
	BufferVertex,
	BufferIndex,
	BufferUniform,
	BufferStorage,
};

enum ShaderType
{
	ShaderTypeVertex,
	ShaderTypeFragment,
	ShaderTypeCompute,
	ShaderTypeCount,
};

enum DataType
{
	DataFloat,
	DataUInt8,
	DataUInt16,
	DataUInt32,
};

enum DrawType
{
	DrawPoints,
	DrawLines,
	DrawTriangles,
};

enum FilterMode
{
	FilterNearest,
	FilterLinear,
};

enum WrapMode
{
	WrapWrap,
	WrapClamp,
	WrapMirror,
};

enum TextureType
{
	Texture1D,
	Texture2D,
	Texture3D,
};

enum TexFormat
{
	TexRGBA8,
	TexDepth32,
};

struct VertexElement
{
	uint32_t Stream;
	uint32_t Index;
	uint32_t NumComponents;
	DataType Type;
	bool Normalized;
	uint32_t Offset;
	uint32_t Stride;
};

struct ShaderSource
{
	ShaderType Type;
	const char *Source;
};

struct SamplerInfo
{
	FilterMode Min, Mag, Mip;
	WrapMode WrapU, WrapV, WrapW;
	uint32_t Anisotropy;
};

struct DepthStencilCreateInfo
{
	TexFormat Format;
	uint32_t Width;
	uint32_t Height;
};

void SetVertexBuffers(CommandBuffer *cb, VertexSpec *spec, Buffer **buffers, uint32_t numStreams);

Buffer *CreateBuffer(BufferType type);
Buffer *CreateStaticBuffer(BufferType type, const void *data, size_t size);
VertexSpec *CreateVertexSpec(const VertexElement *el, uint32_t count);
CommandBuffer *CreateCommandBuffer();

Shader *CreateShader(const ShaderSource *sources, uint32_t numSources);
void DestroyShader(Shader *s);

Texture *CreateTexture(TextureType type);
Texture *CreateStaticTexture2D(const void **data, uint32_t levels, uint32_t width, uint32_t height, TexFormat format);

Framebuffer *CreateFramebuffer(Texture **color, uint32_t numColor, Texture *depthStencilTexture, DepthStencilCreateInfo *depthStencilCreate);

Sampler *CreateSampler(const SamplerInfo *si);

void SetVertexBuffers(CommandBuffer *cb, VertexSpec *spec, Buffer **buffers, uint32_t numStreams);
void SetUniformBuffer(CommandBuffer *cb, uint32_t index, Buffer *b);
void SetShader(CommandBuffer *cb, Shader *s);
void SetIndexBuffer(CommandBuffer *cb, Buffer *b, DataType type);
void SetTexture(CommandBuffer *cb, uint32_t index, Texture *tex, Sampler *sm);
void SetFramebuffer(CommandBuffer *cb, Framebuffer *f);
void DrawIndexed(CommandBuffer *cb, DrawType type, uint32_t num, uint32_t indexOffset);

