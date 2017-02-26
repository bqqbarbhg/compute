#include "particles.h"
#include "renderer.h"
#include "util.h"

namespace {
VertexElement Particle_Elements[] =
{
	{ 0, 0, 2, DataFloat, false, 0*4, 2*4, 0 },
};

float ParticleTexCoords[] =
{
	0.0f, 0.0f,
	1.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 1.0f,
};

uint16_t ParticleIndices[] =
{
	0, 1, 2, 1, 2, 3,
};

struct ParticleUniform
{
	Mat44 u_WorldViewProjection;
};

struct GpuParticle
{
	float PositionAndLifetime[4];
	float Velocity[4];
};

struct GpuTriangle
{
	float P[3][4];
};

struct SimUniform
{
	int u_Counts[4];
	float u_Dt[4];
	float u_Gravity[4];
};

}

class ParticleSystemDumbGpu : public ParticleSystem
{
public:
	std::vector<Particle> NewParticles;

	Texture *ParticleTex;
	Sampler *ParticleSampler;
	VertexSpec *ParticleSpec;
	Buffer *UniformBuffer;
	Buffer *IndexBuffer;

	Shader *ComputeSim;

	Buffer *TexCoordBuffer;
	Buffer *DynamicVertexBuffer;
	RenderState *ParticleState;
	Shader *ParticleShader;

	Buffer *SimUniformBuffer;

	Buffer *NewParticleBuffer;

	Buffer *ParticleBuffer;
	Buffer *TriangleBuffer;

	uint32_t NumParticles;
	uint32_t NumTriangles;

	Timer *GpuTimer, *RenderTimer;

	ParticleSystemDumbGpu()
	{
		ParticleTex = LoadImage("mesh/particle.png");
		ParticleShader = LoadVertFragShader("shader/particle_gpu/particle");
		ComputeSim = LoadComputeShader("shader/particle_gpu/sim");
		ParticleSpec = CreateVertexSpec(Particle_Elements, ArrayCount(Particle_Elements));
		ParticleSampler = CreateSamplerSimple(FilterLinear, FilterLinear, FilterLinear, WrapClamp, 0);
		TexCoordBuffer = CreateStaticBuffer(BufferVertex, ParticleTexCoords, sizeof(ParticleTexCoords));
		IndexBuffer = CreateStaticBuffer(BufferIndex, ParticleIndices, sizeof(ParticleIndices));
		UniformBuffer = CreateStaticBuffer(BufferUniform, NULL, sizeof(ParticleUniform));
		SimUniformBuffer = CreateStaticBuffer(BufferUniform, NULL, sizeof(SimUniform));
		GpuTimer = CreateTimer();
		RenderTimer = CreateTimer();

		ParticleBuffer = CreateStaticBuffer(BufferStorage, NULL, sizeof(GpuParticle) * 1024 * 128);
		TriangleBuffer = CreateBuffer(BufferStorage);
		NewParticleBuffer = CreateBuffer(BufferStorage);

		NumParticles = 0;
		NumTriangles = 0;

		{
			RenderStateInfo rsi = { };
			rsi.DepthTest = RsBoolTrue;
			rsi.DepthWrite = RsBoolFalse;
			rsi.BlendEnable = RsBoolTrue;
			ParticleState = CreateRenderState(&rsi);
		}
	}

	virtual ~ParticleSystemDumbGpu()
	{
	}

	virtual void Initialize(const Triangle *triangles, uint32_t count)
	{
		ReserveUndefinedBuffer(TriangleBuffer, count * sizeof(GpuTriangle), true);
		GpuTriangle *tris = (GpuTriangle*)LockBuffer(TriangleBuffer);

		for (uint32_t i = 0; i < count; i++)
		{
			for (uint32_t p = 0; p < 3; p++)
			{
				tris[i].P[p][0] = triangles[i].P[p].x;
				tris[i].P[p][1] = triangles[i].P[p].y;
				tris[i].P[p][2] = triangles[i].P[p].z;
				tris[i].P[p][3] = 0.0f;
			}
		}

		UnlockBuffer(TriangleBuffer);

		NumTriangles = count;
	}

	virtual void SpawnParticles(const Particle *particles, uint32_t count)
	{
		NewParticles.insert(NewParticles.end(), particles, particles + count);
	}

	virtual void GetParticles(std::vector<Particle>& particles)
	{
		// particles.insert(particles.end(), Particles.begin(), Particles.end());
	}

	virtual void Update(CommandBuffer *cb, float dt)
	{
		char title[128];
		sprintf(title, "Sim: %.2fms, Render: %.2fms   Particles: %u",
			GetTimerMilliseconds(GpuTimer),
			GetTimerMilliseconds(RenderTimer), NumParticles);
		SetWindowTitle(title);

		StartTimer(cb, GpuTimer);

		Vec3 gravity = vec3(0.0f, -4.0f, 0.0f) * dt;

		// Copy new particles
		ReserveUndefinedBuffer(NewParticleBuffer, sizeof(GpuParticle) * NewParticles.size(), false);
		GpuParticle *parts = (GpuParticle*)LockBuffer(NewParticleBuffer);
		for (uint32_t i = 0; i < NewParticles.size(); i++)
		{
			GpuParticle *g = &parts[i];
			Particle *p = &NewParticles[i];

			g->PositionAndLifetime[0] = p->Position.x;
			g->PositionAndLifetime[1] = p->Position.y;
			g->PositionAndLifetime[2] = p->Position.z;
			g->PositionAndLifetime[3] = p->Lifetime;
			g->Velocity[0] = p->Velocity.x;
			g->Velocity[1] = p->Velocity.y;
			g->Velocity[2] = p->Velocity.z;
			g->Velocity[3] = 0.0f;
		}
		UnlockBuffer(NewParticleBuffer);

		CopyBufferData(cb, ParticleBuffer, NewParticleBuffer,
				NumParticles * sizeof(GpuParticle),
				0,
				NewParticles.size() * sizeof(GpuParticle));

		NumParticles += NewParticles.size();
		NewParticles.clear();

		// Simulate particles
		{
			SimUniform u;
			u.u_Counts[0] = NumTriangles;
			u.u_Counts[1] = 0;
			u.u_Counts[2] = 0;
			u.u_Counts[3] = 0;
			u.u_Dt[0] = dt;
			u.u_Dt[1] = 0;
			u.u_Dt[2] = 0;
			u.u_Dt[3] = 0;
			u.u_Gravity[0] = gravity.x;
			u.u_Gravity[1] = gravity.y;
			u.u_Gravity[2] = gravity.z;
			u.u_Gravity[3] = 0.0f;
			SetBufferData(SimUniformBuffer, &u, sizeof(u));
		}

		SetShader(cb, ComputeSim);
		SetUniformBuffer(cb, 0, SimUniformBuffer);
		SetStorageBuffer(cb, 0, ParticleBuffer);
		SetStorageBuffer(cb, 1, TriangleBuffer);
		DispatchCompute(cb, (NumParticles + 63) / 64, 1, 1);

		StopTimer(cb, GpuTimer);
	}

	virtual void Render(CommandBuffer *cb, const Mat44& view, const Mat44& proj)
	{
		StartTimer(cb, RenderTimer);

		// Update uniform buffer
		{
			ParticleUniform u;
			u.u_WorldViewProjection = transpose(view * proj);
			SetBufferData(UniformBuffer, &u, sizeof(u));
		}

		// Set state and draw!
		SetShader(cb, ParticleShader);
		SetRenderState(cb, ParticleState);
		SetUniformBuffer(cb, 0, UniformBuffer);
		SetVertexBuffers(cb, ParticleSpec, &TexCoordBuffer, 1);
		SetIndexBuffer(cb, IndexBuffer, DataUInt16);
		SetStorageBuffer(cb, 0, ParticleBuffer);
		SetTexture(cb, 0, ParticleTex, ParticleSampler);
		DrawIndexedInstanced(cb, DrawTriangles, NumParticles, 6, 0);

		StopTimer(cb, RenderTimer);
	}
};

ParticleSystem *ParticlesCreateDumbGpu()
{
	return new ParticleSystemDumbGpu();
}

