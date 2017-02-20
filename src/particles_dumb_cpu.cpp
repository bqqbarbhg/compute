#include "particles.h"
#include "renderer.h"
#include "util.h"

namespace {

VertexElement Particle_Elements[] =
{
	{ 0, 0, 3, DataFloat, false, 0*4, 3*4, 1 },
	{ 1, 1, 2, DataFloat, false, 0*4, 2*4, 0 },
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

bool RayTriangleIntersect(const Vec3& ro, const Vec3& rd, const Vec3* pV, float *outT)
{
	Vec3 e0 = pV[1] - pV[0];
	Vec3 e1 = pV[2] - pV[0];

	Vec3 p = cross(rd, e1);
	float det = dot(e0, p);

	if (fabsf(det) < 0.0001f)
		return false;

	float inv_det = 1.0f / det;

	Vec3 t = ro - pV[0];
	float u = dot(t, p) * inv_det;
	if (u < 0.0f || u > 1.0f) return false;

	Vec3 q = cross(t, e0);
	float v = dot(rd, q) * inv_det;
	if (v < 0.0f || u + v > 1.0f)
		return false;

	*outT = dot(e1, q) * inv_det;
	return true;
}

};

class ParticleSystemDumbCpu : public ParticleSystem
{
public:
	std::vector<Triangle> Triangles;
	std::vector<Particle> Particles;

	Texture *ParticleTex;
	Sampler *ParticleSampler;
	VertexSpec *ParticleSpec;
	Buffer *UniformBuffer;
	Buffer *IndexBuffer;

	Buffer *TexCoordBuffer;
	Buffer *DynamicVertexBuffer;
	RenderState *ParticleState;
	Shader *ParticleShader;

	Buffer *VertexBuffers[2];

	ParticleSystemDumbCpu()
	{
		ParticleTex = LoadImage("mesh/particle.png");
		ParticleShader = LoadVertFragShader("shader/particle_cpu/particle");
		ParticleSpec = CreateVertexSpec(Particle_Elements, ArrayCount(Particle_Elements));
		ParticleSampler = CreateSamplerSimple(FilterLinear, FilterLinear, FilterLinear, WrapClamp, 0);
		TexCoordBuffer = CreateStaticBuffer(BufferVertex, ParticleTexCoords, sizeof(ParticleTexCoords));
		IndexBuffer = CreateStaticBuffer(BufferIndex, ParticleIndices, sizeof(ParticleIndices));
		UniformBuffer = CreateStaticBuffer(BufferUniform, NULL, sizeof(ParticleUniform));
		DynamicVertexBuffer = CreateBuffer(BufferVertex);

		VertexBuffers[0] = DynamicVertexBuffer;
		VertexBuffers[1] = TexCoordBuffer;

		{
			RenderStateInfo rsi = { };
			rsi.DepthTest = RsBoolTrue;
			rsi.DepthWrite = RsBoolFalse;
			rsi.BlendEnable = RsBoolTrue;
			ParticleState = CreateRenderState(&rsi);
		}
	}

	virtual ~ParticleSystemDumbCpu()
	{
	}

	virtual void Initialize(const Triangle *triangles, uint32_t count)
	{
		Triangles.insert(Triangles.end(), triangles, triangles + count);
	}

	virtual void SpawnParticles(const Particle *particles, uint32_t count)
	{
		Particles.insert(Particles.end(), particles, particles + count);
	}

	virtual void GetParticles(std::vector<Particle>& particles)
	{
		particles.insert(particles.end(), Particles.begin(), Particles.end());
	}

	virtual void Update(CommandBuffer *cb, float dt)
	{
		Vec3 gravity = vec3(0.0f, -4.0f, 0.0f) * dt;
		for (auto &particle : Particles)
		{
			Vec3 prevPos = particle.Position;

			particle.Position += particle.Velocity * dt;
			particle.Velocity += gravity;
			particle.Lifetime -= dt;

			Vec3 delta = particle.Position - prevPos;
			Vec3 dir = normalize(delta);
			float dlen = length(delta);
			for (auto &triangle : Triangles)
			{
				float t = 0.0f;
				if (RayTriangleIntersect(prevPos, dir, triangle.P, &t) && t >= 0.0f && t <= dlen)
				{
					Vec3 n = normalize(cross(triangle.B - triangle.A, triangle.C - triangle.A));
					particle.Position = prevPos;
					particle.Velocity = (particle.Velocity - n * 2.0f * dot(particle.Velocity, n)) * 0.7f;
				}
			}
		}
	}

	virtual void Render(CommandBuffer *cb, const Mat44& view, const Mat44& proj)
	{
		// Update uniform buffer
		{
			ParticleUniform u;
			u.u_WorldViewProjection = transpose(view * proj);
			SetBufferData(UniformBuffer, &u, sizeof(u));
		}

		// Create vertices
		{
			size_t requiredSize = Particles.size() * sizeof(Vec3);
			ReserveUndefinedBuffer(DynamicVertexBuffer, requiredSize, false);

			Vec3 *verts = (Vec3*)LockBuffer(DynamicVertexBuffer);
			for (auto &particle : Particles)
				*verts++ = particle.Position;
			UnlockBuffer(DynamicVertexBuffer);
		}

		// Set state and draw!
		SetShader(cb, ParticleShader);
		SetRenderState(cb, ParticleState);
		SetUniformBuffer(cb, 0, UniformBuffer);
		SetVertexBuffers(cb, ParticleSpec, VertexBuffers, ArrayCount(VertexBuffers));
		SetIndexBuffer(cb, IndexBuffer, DataUInt16);
		SetTexture(cb, 0, ParticleTex, ParticleSampler);
		DrawIndexedInstanced(cb, DrawTriangles, Particles.size(), 6, 0);
	}
};

ParticleSystem *ParticlesCreateDumbCpu()
{
	return new ParticleSystemDumbCpu();
}

