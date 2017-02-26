#pragma once

#include "math.h"
#include <stdint.h>
#include <vector>
#include "intersection.h"

struct CommandBuffer;
struct Texture;

struct Particle
{
	Vec3 Position, Velocity;
	float Lifetime;
};

class ParticleSystem
{
public:
	virtual ~ParticleSystem() { }

	virtual void Initialize(const Triangle *triangles, uint32_t count) = 0;

	virtual void SpawnParticles(const Particle *particles, uint32_t count) = 0;
	virtual void GetParticles(std::vector<Particle>& particles) = 0;

	virtual void Update(CommandBuffer *cb, float dt) = 0;
	virtual void Render(CommandBuffer *cb, const Mat44& view, const Mat44& proj) = 0;
};

ParticleSystem *ParticlesCreateDumbCpu();
ParticleSystem *ParticlesCreateDumbGpu();
ParticleSystem *ParticlesCreateGridGpu();

