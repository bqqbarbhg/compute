#if 0

#include "renderer.h"
#include "../ext/stb_image.h"
#include <vector>
#include <unordered_map>
#include <GLFW/glfw3.h>
#include "math.h"
#include "util.h"
#include "particles.h"

extern GLFWwindow *g_Window;

CommandBuffer *g_CommandBuffer;
VertexSpec *g_ParticleSpec;
Sampler *g_ParticleSampler;
Shader *g_ParticleShader;
RenderState *g_State[2];
Texture *g_ParticleTex;
Texture *g_ParticleTexFront;

Buffer *g_ParticleBuffer;
Buffer *g_IndexBuffer;

Buffer *g_UniformBuffers[32];
uint32_t g_UniformIndex;

struct ParticleVertex
{
	Vec3 pos;
	Vec2 uv;
	float fade;
};

VertexElement ParticleVertex_Elements[] =
{
	{ 0, 0, 3, DataFloat, false, 0*4, 6*4 },
	{ 0, 1, 2, DataFloat, false, 3*4, 6*4 },
	{ 0, 2, 1, DataFloat, false, 5*4, 6*4 },
};

void Initialize()
{
	{
		ShaderSource src[2];
		src[0].Type = ShaderTypeVertex;
		src[0].Source = ReadFile("shader/quad/quad_vert.glsl", NULL);
		src[1].Type = ShaderTypeFragment;
		src[1].Source = ReadFile("shader/quad/quad_frag.glsl", NULL);
		g_ParticleShader = CreateShader(src, 2);
	}

	{
		SamplerInfo si;
		si.Min = si.Mag = si.Mip = FilterLinear;
		si.WrapU = si.WrapV = si.WrapW = WrapWrap;
		si.Anisotropy = 0;
		g_ParticleSampler = CreateSampler(&si);
	}

	g_ParticleSpec = CreateVertexSpec(ParticleVertex_Elements, ArrayCount(ParticleVertex_Elements));
	g_CommandBuffer = CreateCommandBuffer();

	{
		RenderStateInfo rsi;
		rsi.DepthTest = RsBoolFalse;
		rsi.DepthWrite = RsBoolFalse;
		rsi.BlendEnable = RsBoolTrue;
		g_State[0] = CreateRenderState(&rsi);
	}

	{
		RenderStateInfo rsi;
		rsi.DepthTest = RsBoolFalse;
		rsi.DepthWrite = RsBoolFalse;
		rsi.BlendEnable = RsBoolFalse;
		g_State[1] = CreateRenderState(&rsi);
	}

	for (uint32_t i = 0; i < ArrayCount(g_UniformBuffers); i++)
		g_UniformBuffers[i] = CreateBuffer(BufferUniform);

	g_ParticleTex = LoadImage("mesh/orange.png");
	g_ParticleTexFront = LoadImage("mesh/orange_front.png");

	g_ParticleBuffer = CreateBuffer(BufferVertex);

	uint32_t maxQuads = 64;
	uint16_t *particleIndices = new uint16_t[maxQuads * 6];
	for (uint32_t i = 0; i < maxQuads; i++)
	{
		uint16_t *p = particleIndices + i * 6;
		uint16_t b = i * 4;
		p[0] = b + 0; p[1] = b + 1; p[2] = b + 2;
		p[3] = b + 1; p[4] = b + 2; p[5] = b + 3;
	}

	g_IndexBuffer = CreateStaticBuffer(BufferIndex, particleIndices, maxQuads * 6 * 2);
}

void PushUniform(CommandBuffer *cb, uint32_t index, const void *data, uint32_t size)
{
	uint32_t ix = g_UniformIndex;
	g_UniformIndex = (g_UniformIndex + 1) % ArrayCount(g_UniformBuffers);

	Buffer *b = g_UniformBuffers[g_UniformIndex];
	SetBufferData(b, data, size);
	
	SetUniformBuffer(cb, 0, b);
}

struct ParticleUniform
{
	Mat44 u_Projection;
};

void Render()
{
	CommandBuffer *cb = g_CommandBuffer;

	{
		ClearInfo ci;
		ci.ClearColor = true;
		ci.ClearDepth = true;
#if 0
		ci.Color[0] = 0x64 / 255.0f;
		ci.Color[1] = 0x95 / 255.0f;
		ci.Color[2] = 0xED / 255.0f;
#else
		ci.Color[0] = 0x64 / 1255.0f;
		ci.Color[1] = 0x95 / 1255.0f;
		ci.Color[2] = 0xED / 1255.0f;
#endif
		ci.Color[3] = 1.0f;
		ci.Depth = 1.0f;
		Clear(cb, &ci);
	}

	static float TTT;

	float factor = 1.0f;

	if (glfwGetKey(g_Window, GLFW_KEY_A))
		factor = 3.0f;
	if (glfwGetKey(g_Window, GLFW_KEY_Z))
		factor = 0.2f;

	if (glfwGetKey(g_Window, GLFW_KEY_LEFT))
		TTT -= 0.0016f * 5.0f * factor;
	if (glfwGetKey(g_Window, GLFW_KEY_RIGHT))
		TTT += 0.0016f * 5.0f * factor;

	static float HEIGHT = 2.0f;

	if (glfwGetKey(g_Window, GLFW_KEY_UP))
		HEIGHT += 0.05f * factor;
	if (glfwGetKey(g_Window, GLFW_KEY_DOWN))
		HEIGHT -= 0.05f * factor;

	Mat44 proj = mat44_perspective(1.5f, 1280.0f/720.0f, 0.1f, 1000.0f);
	Mat44 view = mat44_lookat(
			vec3(sinf(TTT) * 5.0f, HEIGHT, cosf(TTT) * 5.0f),
			vec3(0.0f, 0.0f, 0.0f),
			vec3(0.0f, 1.0f, 0.0f));


	Mat44 invView = inverse(view);
	
	SetShader(cb, g_ParticleShader);

	Mat44 viewProj = view * proj;
	
	ParticleUniform ou;
	ou.u_Projection = proj;
	PushUniform(cb, 0, &ou, sizeof(ou));
	SetTexture(cb, 0, g_ParticleTex, g_ParticleSampler);
	SetTexture(cb, 1, g_ParticleTexFront, g_ParticleSampler);

#if 0
	Vec3 particles[] = {
		vec3(-2.0f, 0.0f, 0.0f),
		vec3(-1.0f, 0.0f, 0.0f),
		vec3(+0.0f, 0.0f, 0.0f),
		vec3(+1.0f, 0.0f, 0.0f),
		vec3(+2.0f, 0.0f, 0.0f),
	};
#else
	float freq = 3.0f, freq2 = 0.3f;
	Vec3 particles[32]; //??= {
#if 0
		vec3(-2.0f, sinf(TTT * freq + freq2 * 0), 0.0f),
		vec3(-1.0f, sinf(TTT * freq + freq2 * 1), 0.0f),
		vec3(+0.0f, sinf(TTT * freq + freq2 * 2), 0.0f),
		vec3(+1.0f, sinf(TTT * freq + freq2 * 3), 0.0f),
		vec3(+2.0f, sinf(TTT * freq + freq2 * 4), 0.0f),
#endif
	//};

#if 0
	for (int i = 0; i < ArrayCount(particles); i++) {
		particles[i] = vec3(-2.0f + 15.0f * (float)i / (float)ArrayCount(particles), sinf((float)TTT * freq + freq2 * i), 0.0f);
	}
#else
	for (int i = 0; i < ArrayCount(particles); i++) {
		float t = (float)i / (float)(ArrayCount(particles) - 1) * 3.141f * 2.0f;
		particles[i] = vec3(sinf(t), 0.0f, cosf(t)) * 3.0f;
	}
#endif

#endif

	const uint32_t numParticles = ArrayCount(particles);

	size_t requiredSize = numParticles * 4 * sizeof(ParticleVertex);
	ReserveUndefinedBuffer(g_ParticleBuffer, requiredSize, false);

	ParticleVertex *verts = (ParticleVertex*)LockBuffer(g_ParticleBuffer);

	static bool wireframe = false, prevW = false;

	if (glfwGetKey(g_Window, GLFW_KEY_W))
	{
		if (!prevW)
			wireframe = !wireframe;
		prevW = true;
	}
	else
		prevW = false;

	SetRenderState(cb, g_State[wireframe]);
	SetFillMode(cb, wireframe ? FillWireframe : FillSolid);

	float aspect = 1280.0f / 720.0f;
	Vec2 as = vec2(1.0f / aspect, 1.0f);
	Vec2 ias = vec2(1.0f * aspect, 1.0f);

	Vec2 p1;
	float w1;

	for (uint32_t i = 0; i < numParticles; i++)
	{
		ParticleVertex *v = verts + i * 4 - 4;

		Vec3 center = particles[i];
		Vec4 pv4 = vec4(center, 1.0f) * viewProj;
		float w0 = pv4.w;
		Vec2 p0 = vec2(pv4.x, pv4.y) * (1.0f / w0);

		if (i == 0)
		{
			p1 = p0;
			w1 = w0;
		}

		Vec2 a[2];
		Vec2 b[2];

		float r0 = 0.5f / w0;
		float r1 = 0.5f / w1;

		Vec2 dir = (p1 - p0) * ias;
		float len = length(dir);
		if (len > 0.0f)
			dir = dir * (1.0f / len);
		else
			dir = vec2(1.0f, 0.0f);

		float fade = 1.2f - len * 15.0f;

		if (fade < 0.0f) fade = 0.0f;
		if (fade > 1.0f) fade = 1.0f;

		Vec2 up = vec2(dir.y, -dir.x);

		float rad = 0.3f;

		float rf = r0 > r1 ? r0 : r1;
		r0 = r0 * (1.0f - fade) + rf * fade;
		r1 = r1 * (1.0f - fade) + rf * fade;

		float ov = 1.0f + len * 3.0f;
		if (ov > 4.0f) ov = 4.0f;

		float ir0 = 1.0f / r0;
		float ir1 = 1.0f / r1;

		Vec2 po0 = p0 * ir0;
		Vec2 po1 = p1 * ir1;

		a[0] = po0 + (-dir * ov + up) * rad * as;
		a[1] = po0 + (-dir * ov - up) * rad * as;
		b[0] = po1 + (+dir * ov + up) * rad * as;
		b[1] = po1 + (+dir * ov - up) * rad * as;

		p1 = p0;
		w1 = w0;

		if (i == 0)
			continue;

		v[0].pos = vec3(a[0].x, a[0].y, ir0);
		v[1].pos = vec3(a[1].x, a[1].y, ir0);
		v[2].pos = vec3(b[0].x, b[0].y, ir1);
		v[3].pos = vec3(b[1].x, b[1].y, ir1);

		v[0].uv = vec2(0.0f, 0.0f);
		v[1].uv = vec2(0.0f, 1.0f);
		v[2].uv = vec2(1.0f, 0.0f);
		v[3].uv = vec2(1.0f, 1.0f);

		v[0].fade = fade;
		v[1].fade = fade;
		v[2].fade = fade;
		v[3].fade = fade;
	}

	UnlockBuffer(g_ParticleBuffer);

	SetVertexBuffers(cb, g_ParticleSpec, &g_ParticleBuffer, 1);
	SetIndexBuffer(cb, g_IndexBuffer, DataUInt16);
	DrawIndexed(cb, DrawTriangles, (numParticles - 1) * 6, 0);
}


#endif
