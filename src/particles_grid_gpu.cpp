#include "particles.h"
#include "renderer.h"
#include "util.h"
#include "fastmath.h"
#include "intersection.h"

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

const float GridCellSize = 1.0f;
const float ParticleMaxStep = 0.3f;

Vec3 vecMin(const Vec3& a, const Vec3& b)
{
	return vec3(F_MIN(a.x, b.x), F_MIN(a.y, b.y), F_MIN(a.z, b.z));
}

Vec3 vecMax(const Vec3& a, const Vec3& b)
{
	return vec3(F_MAX(a.x, b.x), F_MAX(a.y, b.y), F_MAX(a.z, b.z));
}

struct ParticleUniform
{
	Mat44 u_WorldViewProjection;
};

struct SimUniform
{
	int u_Counts[4];
	int u_Copy[4];
	float u_GridBase[4];
	float u_InvGridSize[4];
	float u_Dt[4];
	float u_Gravity[4];
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

struct GpuCell
{
	int Offsets[4];
};

GpuTriangle toGpu(const Triangle& t)
{
	GpuTriangle gt;
	for (uint32_t p = 0; p < 3; p++)
	{
		gt.P[p][0] = t.P[p].x;
		gt.P[p][1] = t.P[p].y;
		gt.P[p][2] = t.P[p].z;
		gt.P[p][3] = 0.0f;
	}
	return gt;
}

}

class ParticleSystemGridGpu : public ParticleSystem
{
public:
	std::vector<Particle> NewParticles;

	Vec3 GridBase;
	int GridSize[3];

	Texture *ParticleTex;
	Sampler *ParticleSampler;
	VertexSpec *ParticleSpec;
	Buffer *UniformBuffer;
	Buffer *IndexBuffer;
	Buffer *TexCoordBuffer;
	RenderState *ParticleState;
	Shader *ParticleShader;

	Shader *ComputeSim;
	Shader *CopySim;
	Buffer *SimUniformBuffer;

	Buffer *NewParticleBuffer;
	Buffer *ParticleBuffer;
	Buffer *TriangleBuffer;
	Buffer *CellBuffer;

	uint32_t NumParticles;
	uint32_t NumTriangles;

	Timer *GpuTimer, *RenderTimer;

	ParticleSystemGridGpu()
	{
		ParticleTex = LoadImage("mesh/particle.png");
		ParticleShader = LoadVertFragShader("shader/particle_gpu/particle");
		ComputeSim = LoadComputeShader("shader/particle_gpu/cell_sim");
		CopySim = LoadComputeShader("shader/particle_gpu/copy_particles");
		ParticleSpec = CreateVertexSpec(Particle_Elements, ArrayCount(Particle_Elements));
		ParticleSampler = CreateSamplerSimple(FilterLinear, FilterLinear, FilterLinear, WrapClamp, 0);
		TexCoordBuffer = CreateStaticBuffer(BufferVertex, ParticleTexCoords, sizeof(ParticleTexCoords));
		IndexBuffer = CreateStaticBuffer(BufferIndex, ParticleIndices, sizeof(ParticleIndices));
		UniformBuffer = CreateStaticBuffer(BufferUniform, NULL, sizeof(ParticleUniform));
		SimUniformBuffer = CreateStaticBuffer(BufferUniform, NULL, sizeof(SimUniform));
		GpuTimer = CreateTimer();
		RenderTimer = CreateTimer();

		NewParticleBuffer = CreateBuffer(BufferStorage);

		ParticleBuffer = CreateStaticBuffer(BufferStorage, NULL, sizeof(GpuParticle) * 1024 * 512);

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

	virtual ~ParticleSystemGridGpu()
	{
	}

	virtual void Initialize(const Triangle *triangles, uint32_t count)
	{
		if (count == 0)
			return;

		Vec3 min = triangles[0].A, max = min;

		for (uint32_t i = 0; i < count; i++)
		{
			const Triangle *t = &triangles[i];
			min = vecMin(min, vecMin(t->A, vecMin(t->B, t->C)));
			max = vecMax(max, vecMax(t->A, vecMax(t->B, t->C)));
		}

		GridBase = min;
		GridSize[0] = (int)((max.x - min.x) / GridCellSize) + 1;
		GridSize[1] = (int)((max.y - min.y) / GridCellSize) + 1;
		GridSize[2] = (int)((max.z - min.z) / GridCellSize) + 1;

		std::vector<GpuTriangle> gpuTriangles;
		std::vector<GpuCell> cells;

		int cellCount = GridSize[0] * GridSize[1] * GridSize[2];
		for (int cellI = 0; cellI < cellCount; cellI++)
		{
			int x = cellI % GridSize[0];
			int y = cellI / GridSize[0] % GridSize[1];
			int z = cellI / GridSize[0] / GridSize[1];

			size_t cellBase = gpuTriangles.size();

			Vec3 pos = vec3((float)x, (float)y, (float)z);
			Vec3 base = GridBase + pos * GridCellSize;
			Vec3 padding = vec3s(ParticleMaxStep);

			AABB aabb;
			aabb.Min = base - padding;
			aabb.Max = base + vec3s(GridCellSize) + padding;

			for (uint32_t i = 0; i < count; i++)
			{
				AABB ab;
				ab.Min = vecMin(vecMin(triangles[i].A, triangles[i].B), triangles[i].C);
				ab.Max = vecMax(vecMax(triangles[i].A, triangles[i].B), triangles[i].C);
				// if (IntersectAABBvTriangle(aabb, triangles[i]))
				if (IntersectAABBvAABB(aabb, ab))
				{
					gpuTriangles.push_back(toGpu(triangles[i]));
				}
			}

			size_t cellEnd = gpuTriangles.size();

			GpuCell cell;
			cell.Offsets[0] = (uint32_t)cellBase;
			cell.Offsets[1] = (uint32_t)cellEnd;
			cell.Offsets[2] = (uint32_t)(cellEnd - cellBase);
			cell.Offsets[3] = 0;
			cells.push_back(cell);
		}

		TriangleBuffer = CreateStaticBuffer(BufferStorage, gpuTriangles.data(), gpuTriangles.size() * sizeof(GpuTriangle));
		CellBuffer = CreateStaticBuffer(BufferStorage, cells.data(), cells.size() * sizeof(GpuCell));
	}

	virtual void SpawnParticles(const Particle *particles, uint32_t count)
	{
		NewParticles.insert(NewParticles.end(), particles, particles + count);
	}

	virtual void GetParticles(std::vector<Particle>& particles)
	{
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

		uint32_t numNewParticles = 0;
		uint32_t numOldParticles = NumParticles;

		// Copy new particles
		if (!NewParticles.empty())
		{
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

			numNewParticles = NewParticles.size();

			NumParticles += NewParticles.size();
			NewParticles.clear();
		}

		// Simulate particles
		{
			float invCellSize = 1.0f / GridCellSize;

			SimUniform u;
			u.u_Counts[0] = GridSize[0];
			u.u_Counts[1] = GridSize[1];
			u.u_Counts[2] = GridSize[2];
			u.u_Counts[3] = 0;
			u.u_Copy[0] = numOldParticles;
			u.u_Copy[1] = numNewParticles;
			u.u_Copy[2] = 0;
			u.u_Copy[3] = 0;
			u.u_GridBase[0] = GridBase.x;
			u.u_GridBase[1] = GridBase.y;
			u.u_GridBase[2] = GridBase.z;
			u.u_GridBase[3] = 0.0f;
			u.u_InvGridSize[0] = invCellSize;
			u.u_InvGridSize[1] = invCellSize;
			u.u_InvGridSize[2] = invCellSize;
			u.u_InvGridSize[3] = 0.0f;
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

		SetShader(cb, CopySim);
		SetUniformBuffer(cb, 0, SimUniformBuffer);
		SetStorageBuffer(cb, 0, ParticleBuffer);
		SetStorageBuffer(cb, 1, NewParticleBuffer);
		DispatchCompute(cb, (numNewParticles + 63) / 64, 1, 1);

		SetShader(cb, ComputeSim);
		SetUniformBuffer(cb, 0, SimUniformBuffer);
		SetStorageBuffer(cb, 0, ParticleBuffer);
		SetStorageBuffer(cb, 1, TriangleBuffer);
		SetStorageBuffer(cb, 2, CellBuffer);
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

ParticleSystem *ParticlesCreateGridGpu()
{
	return new ParticleSystemGridGpu();
}


