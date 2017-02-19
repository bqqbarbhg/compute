#pragma once

#include <stdint.h>

struct Shader;
struct Buffer;
struct CommandBuffer;
struct VertexSpec;

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

void SetVertexBuffers(CommandBuffer *cb, VertexSpec *spec, Buffer **buffers, uint32_t numStreams);

Buffer *CreateBuffer(BufferType type);
Buffer *CreateStaticBuffer(BufferType type, const void *data, size_t size);
VertexSpec *CreateVertexSpec(const VertexElement *el, uint32_t count);
CommandBuffer *CreateCommandBuffer();

Shader *CreateShader(const ShaderSource *sources, uint32_t numSources);
void DestroyShader(Shader *s);

void SetVertexBuffers(CommandBuffer *cb, VertexSpec *spec, Buffer **buffers, uint32_t numStreams);
void SetUniformBuffer(CommandBuffer *cb, uint32_t index, Buffer *b);
void SetShader(CommandBuffer *cb, Shader *s);
void SetIndexBuffer(CommandBuffer *cb, Buffer *b, DataType type);
void DrawIndexed(CommandBuffer *cb, DrawType type, uint32_t num, uint32_t indexOffset);

