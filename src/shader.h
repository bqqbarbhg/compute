#pragma once

enum ShaderType
{
	ShaderTypeVertex,
	ShaderTypeFragment,
	ShaderTypeCompute,
	ShaderTypeCount,
};

struct Shader;

struct ShaderSource
{
	ShaderType Type;
	const char *Source;
};

void DestroyShader(Shader *s);
Shader *CreateShader(const ShaderSource *sources, uint32_t numSources);
void SetShader(Shader *s);

