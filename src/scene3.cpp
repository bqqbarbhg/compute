#if 1
#include <GLFW/glfw3.h>
#include "renderer.h"
#include "../ext/stb_image.h"
#include "../ext/tinyobj_loader.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "math.h"
#include "intersection.h"
#include "util.h"

extern GLFWwindow *g_Window;

const float Pi = 3.14159265358979323846f;

CommandBuffer *g_CommandBuffer;

VertexSpec *g_ObjSpec;
VertexSpec *g_ReflectorSpec;
VertexSpec *g_LineSpec;
VertexSpec *g_SphereSpec;
VertexSpec *g_QuadSpec;

Sampler *g_BasicSampler;
Sampler *g_ObjSampler;

Shader *g_ObjShader;
Shader *g_ReflectorShader;
Shader *g_LineShader;
Shader *g_ProbeShader;
Shader *g_TonemapShader;

RenderState *g_State;

Texture *g_HdrColor;
Texture *g_HdrDepth;
Framebuffer *g_HdrFramebuffer;

struct ObjVertex
{
	Vec3 pos;
	Vec2 uv;
};

struct LineVertex
{
	Vec3 Pos;
	uint32_t Col;
};

struct SphereVertex
{
	Vec3 Normal;
};

struct ReflectorPair
{
	uint32_t A, B;
	float Distance;
};

constexpr uint32_t MaxGroupNeighbors = 16;

struct ReflectorGroup
{
	std::vector<uint32_t> Reflectors;
	Vec3 Normal;
	float NeighborsInfluence[MaxGroupNeighbors];
	uint32_t Neighbors[MaxGroupNeighbors];
	uint32_t NumNeighbors;
	Vec3 Center;

	Vec3 CurrentLight;
	Vec3 TotalLight;
};

struct GpuReflector
{
	Vec3 Position;
	Vec3 Normal;
	float Radius;
	Vec3 Light;
	uint32_t Color;
};

struct Reflector
{
	Vec3 Position;
	Vec3 Normal;
	float Radius;
	uint32_t Group;

	Vec3 Diffuse;
	Vec3 Emission;
	Vec3 TotalLight;
	Vec3 CurrentLight;

	float NeighborContribution[MaxGroupNeighbors];
};

struct VertexReflector
{
	uint16_t Count;
	uint16_t Index[7];
};

struct Object
{
	std::vector<ObjVertex> Vertices;
	std::vector<uint16_t> Indices;

	Buffer *VertexBuffer;
	Buffer *IndexBuffer;
	Texture *Texture;
	uint32_t NumIndices;

	Buffer *LightBuffer;
	std::vector<VertexReflector> Reflectors;
};

constexpr uint32_t MaxProbeGroups = 32;

struct LightProbeAffectingGroup
{
	uint32_t Index;
	float Influence;
};

struct LightProbe
{
	Vec3 Position;
	Vec3 SH[4];

	LightProbeAffectingGroup Groups[MaxProbeGroups];
	uint32_t NumGroups;
};

std::vector<LightProbe> g_Probes;

Object g_Objects[32];
uint32_t g_NumObjects;

Buffer *g_UniformBuffers[128];
uint32_t g_UniformIndex;

const uint32_t ReflectorSegments = 16;
Buffer *g_ReflectorCircleVertices;
Buffer *g_ReflectorCircleIndices;

Buffer *g_LineBuffer;

const uint32_t SphereRes = 8;
const uint32_t SphereNumIndex = (SphereRes - 1) * (SphereRes - 1) * 6 * 6;
Buffer *g_SphereVertexBuffer;
Buffer *g_SphereIndexBuffer;

Buffer *g_QuadIndices;
Buffer *g_QuadVerts;

std::vector<Reflector> g_Reflectors;
std::vector<ReflectorGroup> g_ReflectorGroups;

Buffer *g_ReflectorBuffer;

struct ObjVertexHash
{
	size_t operator()(const ObjVertex& a) const
	{
		uint32_t *u = (uint32_t*)&a;
		return u[0] ^ u[1] ^ u[2] ^ u[3] ^ u[4];
	}
};

bool operator==(const ObjVertex &a, const ObjVertex &b)
{
	return !memcmp(&a, &b, sizeof(ObjVertex));
}

VertexElement ObjVertex_Elements[] =
{
	{ 0, 0, 3, DataFloat, false, 0*4, 5*4 },
	{ 0, 1, 2, DataFloat, false, 3*4, 5*4 },
	{ 1, 2, 3, DataFloat, false, 0*4, 3*4 },
};

VertexElement ReflectorVertex_Elements[] =
{
	{ 0, 0, 3, DataFloat, false, 0*4, 11*4, 1 },
	{ 0, 1, 3, DataFloat, false, 3*4, 11*4, 1 },
	{ 0, 2, 1, DataFloat, false, 6*4, 11*4, 1 },
	{ 0, 3, 3, DataFloat, false, 7*4, 11*4, 1 },
	{ 0, 4, 4, DataUInt8, true, 10*4, 11*4, 1 },
	{ 1, 5, 2, DataFloat, false, 0*4, 2*4, 0 },
};

VertexElement LineVertex_Elements[] =
{
	{ 0, 0, 3, DataFloat, false, 0*4, 4*4 },
	{ 0, 1, 4, DataUInt8, true,  3*4, 4*4 },
};

VertexElement SphereVertex_Elements[] =
{
	{ 0, 0, 3, DataFloat, false, 0*4, 3*4 },
};

VertexElement QuadVertex_Elements[] =
{
	{ 0, 0, 2, DataFloat, false, 0*4, 4*4 },
	{ 0, 1, 2, DataFloat, false, 2*4, 4*4 },
};

float LightTransportDiscDisc(Reflector &a, Reflector &b)
{
	Vec3 dir = b.Position - a.Position;
	Vec3 ndir = normalize(dir);
	float da = dot(a.Normal, ndir);
	float db = dot(b.Normal, -ndir);
	if (da <= 0.0f || db <= 0.0f)
		return 0.0f;

	float atteunation = 2.0f;
	float inf = (atteunation * da * db) / (Pi * length_squared(dir) + atteunation);
	return inf;
}

float LightTransportDiscPos(Reflector &a, const Vec3& pos)
{
	Vec3 dir = pos - a.Position;
	Vec3 ndir = normalize(dir);
	float da = dot(a.Normal, ndir);
	if (da <= 0.0f)
		return 0.0f;

	float atteunation = 2.0f;
	float inf = (atteunation * da) / (Pi * length_squared(dir) + atteunation);
	return inf;
}

std::vector<LineVertex> g_DebugLines;

void DebugLine(Vec3 a, Vec3 b, Vec3 color)
{
	LineVertex av, bv;
	av.Pos = a; 
	bv.Pos = b; 

	g_DebugLines.push_back(av);
	g_DebugLines.push_back(bv);
}

void AddLightToSH(Vec3 *sh, const Vec3& light, const Vec3& dir)
{
	const float p00 = 0.282094791773878140f;
	const float p01 = 0.488602511902919920f;

	sh[0] += light * (p00);
	sh[1] += light * (p01 * dir.x);
	sh[2] += light * (p01 * dir.y);
	sh[3] += light * (p01 * dir.z);
}

void Initialize()
{
	{
		ShaderSource src[2];
		src[0].Type = ShaderTypeVertex;
		src[0].Source = ReadFile("shader/light/object_vert.glsl", NULL);
		src[1].Type = ShaderTypeFragment;
		src[1].Source = ReadFile("shader/light/object_frag.glsl", NULL);
		g_ObjShader = CreateShader(src, 2);
	}

	{
		ShaderSource src[2];
		src[0].Type = ShaderTypeVertex;
		src[0].Source = ReadFile("shader/light/reflector_debug_vert.glsl", NULL);
		src[1].Type = ShaderTypeFragment;
		src[1].Source = ReadFile("shader/light/reflector_debug_frag.glsl", NULL);
		g_ReflectorShader = CreateShader(src, 2);
	}

	{
		ShaderSource src[2];
		src[0].Type = ShaderTypeVertex;
		src[0].Source = ReadFile("shader/test/debug_line_vert.glsl", NULL);
		src[1].Type = ShaderTypeFragment;
		src[1].Source = ReadFile("shader/test/debug_line_frag.glsl", NULL);
		g_LineShader = CreateShader(src, 2);
	}

	{
		ShaderSource src[2];
		src[0].Type = ShaderTypeVertex;
		src[0].Source = ReadFile("shader/light/probe_debug_vert.glsl", NULL);
		src[1].Type = ShaderTypeFragment;
		src[1].Source = ReadFile("shader/light/probe_debug_frag.glsl", NULL);
		g_ProbeShader = CreateShader(src, 2);
	}

	{
		ShaderSource src[2];
		src[0].Type = ShaderTypeVertex;
		src[0].Source = ReadFile("shader/light/tonemap_vert.glsl", NULL);
		src[1].Type = ShaderTypeFragment;
		src[1].Source = ReadFile("shader/light/tonemap_frag.glsl", NULL);
		g_TonemapShader = CreateShader(src, 2);
	}

	{
		SamplerInfo si;
		si.Min = si.Mag = si.Mip = FilterLinear;
		si.WrapU = si.WrapV = si.WrapW = WrapWrap;
		si.Anisotropy = 0;
		g_BasicSampler = CreateSampler(&si);
	}

	{
		SamplerInfo si;
		si.Min = si.Mag = si.Mip = FilterLinear;
		si.WrapU = si.WrapV = si.WrapW = WrapWrap;
		si.Anisotropy = 0;
		g_ObjSampler = CreateSampler(&si);
	}

	g_ObjSpec = CreateVertexSpec(ObjVertex_Elements, ArrayCount(ObjVertex_Elements));
	g_ReflectorSpec = CreateVertexSpec(ReflectorVertex_Elements, ArrayCount(ReflectorVertex_Elements));
	g_LineSpec = CreateVertexSpec(LineVertex_Elements, ArrayCount(LineVertex_Elements));
	g_SphereSpec = CreateVertexSpec(SphereVertex_Elements, ArrayCount(SphereVertex_Elements));
	g_QuadSpec = CreateVertexSpec(QuadVertex_Elements, ArrayCount(QuadVertex_Elements));

	g_CommandBuffer = CreateCommandBuffer();


	{
		RenderStateInfo rsi;
		rsi.DepthTest = RsBoolTrue;
		rsi.DepthWrite = RsBoolTrue;
		rsi.BlendEnable = RsBoolFalse;
		g_State = CreateRenderState(&rsi);
	}

	for (uint32_t i = 0; i < ArrayCount(g_UniformBuffers); i++)
		g_UniformBuffers[i] = CreateBuffer(BufferUniform);

	g_LineBuffer = CreateBuffer(BufferVertex);

	g_HdrColor = CreateStaticTexture2D(NULL, 1, 1280, 720, TexRGBAF16);
	g_HdrDepth = CreateStaticTexture2D(NULL, 1, 1280, 720, TexDepth32);

	g_HdrFramebuffer = CreateFramebuffer(&g_HdrColor, 1, g_HdrDepth, NULL);

	std::vector<Triangle> triangles;

	{
		int imageWidth, imageHeight;
		stbi_uc *image = stbi_load("mesh/room.png", &imageWidth, &imageHeight, 0, 4);

		Texture *texture = CreateStaticTexture2D((const void**)&image, 1, imageWidth, imageHeight, TexRGBA8);

		std::string inputfile = "mesh/room.obj";
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		std::string err;
		bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, inputfile.c_str());

		g_NumObjects = (uint32_t)shapes.size();

		for (size_t shapeI = 0; shapeI < shapes.size(); shapeI++)
		{
			std::vector<ObjVertex> vertices;
			std::vector<uint16_t> indices;
			std::unordered_map<ObjVertex, uint32_t, ObjVertexHash> indexMap;

			tinyobj::shape_t *shape = &shapes[shapeI];
			size_t indexOffset = 0;
			for (size_t faceI = 0; faceI < shape->mesh.num_face_vertices.size(); faceI++)
			{
				Vec3 verts[3];

				assert(shape->mesh.num_face_vertices[faceI] == 3);
				for (uint32_t i = 0; i < 3; i++)
				{
					tinyobj::index_t idx = shape->mesh.indices[indexOffset + i];

					float *vv = &attrib.vertices[idx.vertex_index * 3];
					float *vt = &attrib.texcoords[idx.texcoord_index * 2];

					ObjVertex v;
					v.pos.x = vv[0];
					v.pos.y = vv[1];
					v.pos.z = vv[2];
					v.uv.x = vt[0];
					v.uv.y = 1.0f - vt[1];

					verts[i].x = v.pos.x;
					verts[i].y = v.pos.y;
					verts[i].z = v.pos.z;

					uint16_t index = (uint16_t)vertices.size();
					auto it = indexMap.insert(std::make_pair(v, index));
					if (it.second)
					{
						indices.push_back(index);
						vertices.push_back(v);
					}
					else
					{
						indices.push_back(it.first->second);
					}
				}

				indexOffset += 3;

				Triangle t;
				t.A = verts[0];
				t.B = verts[1];
				t.C = verts[2];
				triangles.push_back(t);
			}

			Object *obj = &g_Objects[shapeI];
			obj->VertexBuffer = CreateStaticBuffer(BufferVertex,
				vertices.data(), vertices.size() * sizeof(ObjVertex));
			obj->IndexBuffer = CreateStaticBuffer(BufferIndex,
				indices.data(), indices.size() * sizeof(uint16_t));
			obj->Texture = texture;

			obj->NumIndices = (uint32_t)indices.size();

			obj->LightBuffer = CreateBuffer(BufferVertex);

			obj->Reflectors.resize(vertices.size());
			memset(obj->Reflectors.data(), 0, sizeof(VertexReflector) * obj->Reflectors.size());

			obj->Vertices.swap(vertices);
			obj->Indices.swap(indices);
		}
	}

	{
		Vec2 verts[ReflectorSegments + 1];
		uint16_t indices[ReflectorSegments * 3];

		verts[ReflectorSegments] = vec2(0.0f, 0.0f);
		for (uint32_t i = 0; i < ReflectorSegments; i++)
		{
			float t = (float)i / (float)ReflectorSegments * Pi * 2.0f;
			verts[i] = vec2(cosf(t), sinf(t));

			indices[i * 3 + 0] = ReflectorSegments;
			indices[i * 3 + 1] = i;
			indices[i * 3 + 2] = (i + 1) % ReflectorSegments;
		}

		g_ReflectorCircleVertices = CreateStaticBuffer(BufferVertex, verts, ReflectorSegments * sizeof(Vec2));
		g_ReflectorCircleIndices = CreateStaticBuffer(BufferIndex, indices, ReflectorSegments * sizeof(uint16_t) * 3);
	}

	{
		std::vector<SphereVertex> verts;
		std::vector<uint16_t> indices;
		verts.resize(SphereRes * SphereRes * 6);
		indices.resize((SphereRes - 1) * (SphereRes - 1) * 6 * 6);

		Vec3 axes[3] = {
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f },
		};

		for (int axis = 0; axis < 3; axis++)
		{
			Vec3 u = axes[(axis + 0) % 3];
			Vec3 r = axes[(axis + 1) % 3];
			Vec3 f = axes[(axis + 2) % 3];

			for (int side = 0; side < 2; side++)
			{
				Vec3 center = f * (side ? 0.5f : -0.5f);

				for (int y = 0; y < SphereRes; y++)
				for (int x = 0; x < SphereRes; x++)
				{
					float xf = x / (float)(SphereRes - 1) - 0.5f;
					float yf = y / (float)(SphereRes - 1) - 0.5f;
					Vec3 n = normalize(center + r * xf + u * yf);

					verts[((axis * 2 + side) * SphereRes + y) * SphereRes + x].Normal = n;
				}
			}
		}

		for (int face = 0; face < 6; face++)
		{
			uint16_t base = face * SphereRes * SphereRes;
			uint16_t *fi = indices.data() + face * (SphereRes - 1) * (SphereRes - 1) * 6;

			for (uint32_t y = 0; y < SphereRes - 1; y++)
			for (uint32_t x = 0; x < SphereRes - 1; x++)
			{
				uint16_t a = base + y * SphereRes + x, b = a + SphereRes;
				uint16_t *qi = fi + (y * (SphereRes - 1) + x) * 6;

				qi[0] = a;
				qi[1] = a + 1;
				qi[2] = b;
				qi[3] = b;
				qi[4] = a + 1;
				qi[5] = b + 1;
			}
		}

		g_SphereVertexBuffer = CreateStaticBuffer(BufferVertex, verts.data(), verts.size() * sizeof(SphereVertex));
		g_SphereIndexBuffer = CreateStaticBuffer(BufferIndex, indices.data(), indices.size() * sizeof(uint16_t));
	}

	{
		Vec2 vert[] = {
			{ -1.0f, -1.0f }, { 0.0f, 0.0f },
			{ +1.0f, -1.0f }, { 1.0f, 0.0f },
			{ -1.0f, +1.0f }, { 0.0f, 1.0f },
			{ +1.0f, +1.0f }, { 1.0f, 1.0f },
		};
		uint16_t index[] = { 0, 1, 2, 2, 1, 3 };

		g_QuadVerts = CreateStaticBuffer(BufferVertex, vert, sizeof(vert));
		g_QuadIndices = CreateStaticBuffer(BufferIndex, index, sizeof(index));
	}

	{
		std::vector<Reflector> reflectors;
		std::vector<ReflectorPair> pairs;
		std::vector<ReflectorGroup> groups;
		uint32_t groupedCount = 0;

		std::vector<std::vector<uint32_t> > neighbors;

		std::vector<float> groupInfluence;

		for (uint32_t objI = 0; objI < g_NumObjects; objI++)
		{
			Object *obj = &g_Objects[objI];
			
			for (uint32_t indexI = 0; indexI < obj->Indices.size(); indexI += 3)
			{
				uint16_t *ix = obj->Indices.data() + indexI;

				Vec3 a = obj->Vertices[ix[0]].pos;
				Vec3 b = obj->Vertices[ix[1]].pos;
				Vec3 c = obj->Vertices[ix[2]].pos;

				Vec3 average = (a + b + c) * (1.0f / 3.0f);
				Vec3 normal = normalize(cross(b - a, c - a));

				float dA = length(a - average);
				float dB = length(a - average);
				float dC = length(a - average);

				Reflector r;
				r.Position = average;
				r.Normal = normal;
				r.Radius = (dA + dB + dC) * (1.0f / 3.0f) * 0.5f;
				r.Group = ~0U;
				for (uint32_t i = 0; i < MaxGroupNeighbors; i++)
					r.NeighborContribution[i] = 0.0f;

#if 0
				if (r.Normal.x > 0.9f && r.Position.x < -3.0f)
					r.Diffuse = vec3(1.0f, 0.0f, 0.0f) * 0.7f;
				else if (r.Normal.x < -0.9f && r.Position.x > 3.0f)
					r.Diffuse = vec3(0.0f, 1.0f, 0.0f) * 0.7f;
				else
#endif
					r.Diffuse = vec3(1.0f, 1.0f, 1.0f) * 0.7f;

				uint32_t ri = reflectors.size();
				reflectors.push_back(r);

				for (uint32_t i = 0; i < 3; i++)
				{
					VertexReflector &vr = obj->Reflectors[ix[i]];
					if (vr.Count < ArrayCount(vr.Index))
						vr.Index[vr.Count++] = (uint16_t)ri;
				}
			}
		}

		neighbors.resize(reflectors.size());

		while (groupedCount < reflectors.size())
		{
			uint32_t first;
			for (first = 0; first < reflectors.size(); first++)
			{
				if (reflectors[first].Group == ~0U)
					break;
			}

			Vec3 average = reflectors[first].Position;
			float groupPlane = dot(average, reflectors[first].Normal);

			uint32_t groupIx = groups.size();
			groups.emplace_back();
			ReflectorGroup &group = groups.back();

			reflectors[first].Group = groupIx;
			group.Reflectors.push_back(first);
			groupedCount++;

			group.Normal = reflectors[first].Normal;

			for (int addI = 0; addI < 128; addI++)
			{
				uint32_t closest = ~0U;
				const float closestRadius = 3.0f;
				float closestDist = closestRadius * closestRadius;
				for (uint32_t i = 0; i < reflectors.size(); i++)
				{
					if (reflectors[i].Group != ~0U)
						continue;

					if (fabsf(dot(group.Normal, reflectors[i].Position) - groupPlane) > 0.1f)
						continue;

					if (dot(reflectors[i].Normal, group.Normal) < 0.95f)
						continue;

					float distSq = length_squared(average - reflectors[i].Position);
					if (distSq < closestDist)
					{
						closest = i;
						closestDist = distSq;
					}
				}

				if (closest == ~0U)
					break;

				reflectors[closest].Group = groupIx;
				group.Reflectors.push_back(closest);
				groupedCount++;
			}
			
			Vec3 center = vec3_zero;
			for (uint32_t i = 0; i < group.Reflectors.size(); i++)
				center += reflectors[group.Reflectors[i]].Position;
			group.Center = center * (1.0f / (float)group.Reflectors.size());
		}

		groupInfluence.resize(groups.size() * groups.size());

		for (uint32_t aI = 0; aI < reflectors.size(); aI++)
		{
			for (uint32_t bI = aI + 1; bI < reflectors.size(); bI++)
			{
				Reflector &a = reflectors[aI];
				Reflector &b = reflectors[bI];

				float transport = LightTransportDiscDisc(a, b);

				if (transport > 0.001f)
				{
					Vec3 src = a.Position + a.Normal * 0.01f;
					Vec3 dst = b.Position + b.Normal * 0.01f;
					float len = length(dst - src);
					Vec3 dir = normalize(dst - src);
					bool shadow = false;

					for (Triangle &tri : triangles)
					{
						float t;
						if (IntersectRayvTriangle(src, dir, tri, &t))
						{
							if (t > 0.0f && t < len)
							{
								shadow = true;
								break;
							}
						}
					}

					if (shadow)
						continue;

					ReflectorPair pair;
					pair.A = aI;
					pair.B = bI;
					pair.Distance = length(a.Position - b.Position);
					pairs.push_back(pair);


					{
						neighbors[aI].push_back(bI);
						neighbors[bI].push_back(aI);
					}

					{
						groupInfluence[a.Group * groups.size() + b.Group] += transport;
						groupInfluence[b.Group * groups.size() + a.Group] += transport;
					}
				}
			}
		}

		std::vector<std::pair<float, uint32_t> > sortedInfluences;
		sortedInfluences.reserve(groups.size());
		for (uint32_t groupI = 0; groupI < groups.size(); groupI++)
		{
			ReflectorGroup &group = groups[groupI];

			float *influence = groupInfluence.data() + groupI * groups.size();
			sortedInfluences.clear();
			for (uint32_t influenceI = 0; influenceI < groups.size(); influenceI++)
			{
				if (influence[influenceI] > 0.01f)
					sortedInfluences.push_back(std::make_pair(influence[influenceI], influenceI));
			}

			std::sort(sortedInfluences.begin(), sortedInfluences.end(), [](auto &a, auto &b){
				return a.first > b.first;
			});

			if (sortedInfluences.size() > MaxGroupNeighbors)
				sortedInfluences.resize(MaxGroupNeighbors);

			for (uint32_t i = 0; i < sortedInfluences.size(); i++)
			{
				group.Neighbors[i] = sortedInfluences[i].second;
				group.NeighborsInfluence[i] = 0.0f;
			}
			group.NumNeighbors = sortedInfluences.size();
		}

		for (uint32_t groupI = 0; groupI < groups.size(); groupI++)
		{
			ReflectorGroup &group = groups[groupI];

			for (uint32_t grI = 0; grI < group.Reflectors.size(); grI++)
			{
				uint32_t refI = group.Reflectors[grI];
				auto &refNb = neighbors[refI];

				float *influence = reflectors[refI].NeighborContribution;

				for (auto nb : refNb)
				{
					uint32_t i;
					uint32_t nbg = reflectors[nb].Group;
					if (nbg == groupI)
						continue;

					for (i = 0; i < group.NumNeighbors; i++)
					{
						if (nbg == group.Neighbors[i])
							break;
					}

					if (i < group.NumNeighbors)
					{
						Reflector &a = reflectors[refI];
						Reflector &b = reflectors[nb];

						influence[i] += LightTransportDiscDisc(a, b);
					}
				}

				for (uint32_t i = 0; i < group.NumNeighbors; i++)
					group.NeighborsInfluence[i] += influence[i];
			}

			for (uint32_t i = 0; i < group.NumNeighbors; i++)
				group.NeighborsInfluence[i] /= (float)group.Reflectors.size();

		}

		g_Reflectors.swap(reflectors);
		g_ReflectorGroups.swap(groups);
		g_ReflectorBuffer = CreateBuffer(BufferVertex);
	}

	{
		LightProbe probe;
		probe.Position = vec3(0.0f, 2.0f, 0.0f);
		memset(probe.SH, 0, sizeof(probe.SH));
		g_Probes.push_back(probe);
	}

	std::vector<std::pair<float, uint32_t> > groupInfluence;
	groupInfluence.resize(g_ReflectorGroups.size());
	for (uint32_t pI = 0; pI < g_Probes.size(); pI++)
	{
		LightProbe &p = g_Probes[pI];
		for (uint32_t i = 0; i < g_ReflectorGroups.size(); i++)
			groupInfluence[i] = std::make_pair(0.0f, i);

		for (uint32_t aI = 0; aI < g_Reflectors.size(); aI++)
		{
			Reflector &a = g_Reflectors[aI];

			float transport = LightTransportDiscPos(a, p.Position);

			if (transport > 0.001f)
			{
				Vec3 src = a.Position;
				Vec3 dst = p.Position;
				float len = length(dst - src);
				Vec3 dir = normalize(dst - src);

				bool shadow = false;

				for (Triangle &tri : triangles)
				{
					float t;
					if (IntersectRayvTriangle(src, dir, tri, &t))
					{
						if (t > 0.01f && t < len - 0.01f)
						{
							shadow = true;
							break;
						}
					}
				}

				if (shadow)
					continue;

				groupInfluence[a.Group].first += transport;
			}
		}

		std::sort(groupInfluence.begin(), groupInfluence.end(), [](auto &a, auto &b){
			return a.first > b.first;
		});

		uint32_t num;
		for (num = 0; num < MaxProbeGroups; num++)
		{
			float influence = groupInfluence[num].first;
			if (influence <= 0.001f)
				break;

			p.Groups[num].Index = groupInfluence[num].second;
			p.Groups[num].Influence = groupInfluence[num].first;
		}
		p.NumGroups = num;
	}
}

void PushUniform(CommandBuffer *cb, uint32_t index, const void *data, uint32_t size)
{
	uint32_t ix = g_UniformIndex;
	g_UniformIndex = (g_UniformIndex + 1) % ArrayCount(g_UniformBuffers);

	Buffer *b = g_UniformBuffers[g_UniformIndex];
	SetBufferData(b, data, size);

	SetUniformBuffer(cb, index, b);
}

struct ObjectUniform
{
	Mat44 u_WorldViewProjection;
};

struct ReflectorUniform
{
	Mat44 u_ViewProjection;
};

struct LineUniform
{
	Mat44 u_ViewProjection;
};

struct ProbeUniformGlobal
{
	Mat44 u_ViewProjection;
};

struct ProbeUniform
{
	Vec4 u_PositionRadius;
	Vec4 u_SH[4];
};

bool g_TogglePrev[512];
bool g_ToggleValue[512];

bool Toggle(int key)
{
	if (glfwGetKey(g_Window, key))
	{
		if (!g_TogglePrev[key])
			g_ToggleValue[key] = !g_ToggleValue[key];
		g_TogglePrev[key] = true;
	}
	else
		g_TogglePrev[key] = false;
	return g_ToggleValue[key];
}

void UpdateLight()
{
	auto &reflectors = g_Reflectors;
	auto &groups = g_ReflectorGroups;

	static float TTT = 0.0f;
	TTT += 0.016f;

	for (uint32_t groupI = 0; groupI < groups.size(); groupI++)
	{
		ReflectorGroup &group = groups[groupI];
		group.TotalLight = vec3(0.0f, 0.0f, 0.0f);
		group.CurrentLight = vec3(0.0f, 0.0f, 0.0f);
	}

	for (uint32_t i = 0; i < reflectors.size(); i++)
	{
		Reflector &r = reflectors[i];

		Vec2 p = vec2(r.Position.x, r.Position.z);
		Vec2 l = vec2(sinf(TTT) * 2.0f, cosf(TTT) * 2.0f);

		float light = 1.0f - length_squared(p - l) * 0.3f;
		if (light < 0.0f) light = 0.0f;

		if (r.Position.y < 4.0f)
			light = 0.0f;

		r.CurrentLight = r.Diffuse * vec3s(light);
		r.TotalLight = r.CurrentLight;
	}

	bool separate = !Toggle(GLFW_KEY_S);

	for (int bounce = 0; bounce < 3; bounce++)
	{
		for (uint32_t groupI = 0; groupI < groups.size(); groupI++)
		{
			ReflectorGroup &group = groups[groupI];
			Vec3 total = vec3_zero;
			for (uint32_t grI = 0; grI < group.Reflectors.size(); grI++)
			{
				Reflector &ref = reflectors[group.Reflectors[grI]];
				total += ref.CurrentLight;
			}
			group.CurrentLight = total * (1.0f / (float)group.Reflectors.size());
			group.TotalLight += group.CurrentLight;
		}

		if (separate)
		{
			for (uint32_t groupI = 0; groupI < groups.size(); groupI++)
			{
				ReflectorGroup &group = groups[groupI];
				Vec3 influence[MaxGroupNeighbors];
				memset(influence, 0, sizeof(influence));

				for (uint32_t nbI = 0; nbI < group.NumNeighbors; nbI++)
				{
					influence[nbI] = groups[group.Neighbors[nbI]].CurrentLight;
				}

				for (uint32_t grI = 0; grI < group.Reflectors.size(); grI++)
				{
					Reflector &ref = reflectors[group.Reflectors[grI]];
					ref.CurrentLight = vec3s(0.0f);
					for (uint32_t nbI = 0; nbI < group.NumNeighbors; nbI++)
					{
						Vec3 light = influence[nbI] * ref.NeighborContribution[nbI];
						light = ref.Diffuse * light;
						ref.CurrentLight += light;
						ref.TotalLight += light;
					}
				}
			}
		}
		else
		{
			for (uint32_t groupI = 0; groupI < groups.size(); groupI++)
			{
				ReflectorGroup &group = groups[groupI];
				Vec3 influence = vec3_zero;

				for (uint32_t nbI = 0; nbI < group.NumNeighbors; nbI++)
				{
					influence += group.NeighborsInfluence[nbI] * groups[group.Neighbors[nbI]].CurrentLight;
				}

				for (uint32_t grI = 0; grI < group.Reflectors.size(); grI++)
				{
					Reflector &ref = reflectors[group.Reflectors[grI]];
					ref.CurrentLight = vec3s(0.0f);
					Vec3 light = influence;
					light = ref.Diffuse * light;
					ref.CurrentLight += light;
					ref.TotalLight += light;
				}
			}
		}
	}

	for (uint32_t probeI = 0; probeI < g_Probes.size(); probeI++)
	{
		LightProbe &probe = g_Probes[probeI];
		memset(probe.SH, 0, sizeof(probe.SH));

		for (uint32_t groupI = 0; groupI < probe.NumGroups; groupI++)
		{
			ReflectorGroup &group = groups[probe.Groups[groupI].Index];
			Vec3 diff = group.Center - probe.Position;
			Vec3 normal = normalize(diff);
			Vec3 light = group.TotalLight * probe.Groups[groupI].Influence;

			AddLightToSH(probe.SH, light, normal);
		}
	}

#if 0
	for (uint32_t groupI = 0; groupI < groups.size(); groupI++)
	{
		ReflectorGroup &a = groups[groupI];
		for (uint32_t nbI = 0; nbI < a.NumNeighbors; nbI++)
		{
			ReflectorGroup &b = groups[a.Neighbors[nbI]];
			DebugLine(a.Center, b.Center, vec3(1.0f, 1.0f, 1.0f));
		}
	}
#endif
}

void Render()
{
	CommandBuffer *cb = g_CommandBuffer;

	UpdateLight();

	SetRenderState(cb, g_State);

	static float TTT = 3.0f;

	if (glfwGetKey(g_Window, GLFW_KEY_RIGHT))
		TTT += 0.0016f * 5.0f;
	else if (glfwGetKey(g_Window, GLFW_KEY_LEFT))
		TTT -= 0.0016f * 5.0f;

	Mat44 proj = mat44_perspective(1.5f, 1280.0f/720.0f, 0.1f, 1000.0f);
	Mat44 view = mat44_lookat(
		vec3(sinf(TTT) * 3.0f, 3.0f, cosf(TTT) * 3.0f),
		vec3(0.0f, 0.0f, 0.0f),
		vec3(0.0f, 1.0f, 0.0f));

	bool renderReflectors = Toggle(GLFW_KEY_SPACE);

	SetFramebuffer(cb, g_HdrFramebuffer);

	{
		ClearInfo ci;
		ci.ClearColor = true;
		ci.ClearDepth = true;
		ci.Color[0] = 0x64 / 255.0f;
		ci.Color[1] = 0x95 / 255.0f;
		ci.Color[2] = 0xED / 255.0f;
		ci.Color[3] = 1.0f;
		ci.Depth = 1.0f;
		Clear(cb, &ci);
	}

	if (!renderReflectors)
	{
		SetShader(cb, g_ObjShader);

		ObjectUniform ou;
		ou.u_WorldViewProjection = transpose(view * proj);
		PushUniform(cb, 0, &ou, sizeof(ou));

		for (uint32_t objI = 0; objI < g_NumObjects; objI++)
		{
			Object *obj = &g_Objects[objI];

			ReserveUndefinedBuffer(obj->LightBuffer, sizeof(Vec3) * obj->Reflectors.size(), false);
			Vec3 *light = (Vec3*)LockBuffer(obj->LightBuffer);
			for (uint32_t ri = 0; ri < obj->Reflectors.size(); ri++)
			{
				VertexReflector &vr = obj->Reflectors[ri];
				Vec3 total = vec3s(0.0f);
				for (uint32_t i = 0; i < vr.Count; i++)
					total += g_Reflectors[vr.Index[i]].TotalLight;
				light[ri] = total * (1.0f / (float)vr.Count);
			}
			UnlockBuffer(obj->LightBuffer);

			Buffer *streams[2] = { obj->VertexBuffer, obj->LightBuffer };
			SetVertexBuffers(cb, g_ObjSpec, streams, 2);

			SetIndexBuffer(cb, obj->IndexBuffer, DataUInt16);
			SetTexture(cb, 0, obj->Texture, g_ObjSampler);
			DrawIndexed(cb, DrawTriangles, obj->NumIndices, 0);
		}
	}

	if (renderReflectors)
	{
		SetShader(cb, g_ReflectorShader);

		ReflectorUniform ou;
		ou.u_ViewProjection = transpose(view * proj);
		PushUniform(cb, 0, &ou, sizeof(ou));

		uint32_t palette[3 * 3 * 3];
		for (uint32_t ix = 0; ix < 3 * 3 * 3; ix++)
		{
			uint32_t r = ix % 3 * 60;
			uint32_t g = ix / 3 % 3 * 60;
			uint32_t b = ix / 3 / 3 % 3 * 60;
			palette[ix] = r | g << 8 | b << 16;
		}

		ReserveUndefinedBuffer(g_ReflectorBuffer, sizeof(GpuReflector) * g_Reflectors.size(), false);
		GpuReflector *ref = (GpuReflector*)LockBuffer(g_ReflectorBuffer);

		for (uint32_t i = 0; i < g_Reflectors.size(); i++)
		{
			ref[i].Position = g_Reflectors[i].Position + g_Reflectors[i].Normal * (float)i * 0.0001f;
			ref[i].Normal = g_Reflectors[i].Normal;
			ref[i].Radius = g_Reflectors[i].Radius;
			ref[i].Light = g_Reflectors[i].TotalLight;
			ref[i].Color = palette[g_Reflectors[i].Group % ArrayCount(palette)];
		}

		UnlockBuffer(g_ReflectorBuffer);

		Buffer *streams[2] = { g_ReflectorBuffer, g_ReflectorCircleVertices };
		SetVertexBuffers(cb, g_ReflectorSpec, streams, 2);

		SetIndexBuffer(cb, g_ReflectorCircleIndices, DataUInt16);

		DrawIndexedInstanced(cb, DrawTriangles, g_Reflectors.size(), ReflectorSegments * 3, 0);
	}

	if (1)
	{
		SetShader(cb, g_ProbeShader);

		ProbeUniformGlobal gu;
		gu.u_ViewProjection = transpose(view * proj);
		PushUniform(cb, 0, &gu, sizeof(gu));

		SetVertexBuffers(cb, g_SphereSpec, &g_SphereVertexBuffer, 1);
		SetIndexBuffer(cb, g_SphereIndexBuffer, DataUInt16);

		for (auto &probe : g_Probes)
		{
			ProbeUniform ou;
			ou.u_PositionRadius = vec4(probe.Position, 0.15f);

			for (int band = 0; band < 4; band++)
				ou.u_SH[band] = vec4(probe.SH[band], 0.0f);

			PushUniform(cb, 1, &ou, sizeof(ou));

			DrawIndexed(cb, DrawTriangles, SphereNumIndex, 0);
		}
	}
	
	if (0)
	{
		if (g_DebugLines.size() > 0)
		{
			SetShader(cb, g_LineShader);

			LineUniform ou;
			ou.u_ViewProjection = transpose(view * proj);
			PushUniform(cb, 0, &ou, sizeof(ou));

			ReserveUndefinedBuffer(g_LineBuffer, sizeof(LineVertex) * g_DebugLines.size(), false);
			void *ptr = LockBuffer(g_LineBuffer);
			memcpy(ptr, g_DebugLines.data(), g_DebugLines.size() * sizeof(LineVertex));
			UnlockBuffer(g_LineBuffer);

			SetVertexBuffers(cb, g_LineSpec, &g_LineBuffer, 1);
			DrawArrays(cb, DrawLines, g_DebugLines.size(), 0);
		}
	}
	g_DebugLines.clear();

	SetFramebuffer(cb, NULL);

	{
		ClearInfo ci;
		ci.ClearColor = true;
		ci.ClearDepth = true;
		ci.Color[0] = 0x64 / 255.0f;
		ci.Color[1] = 0x95 / 255.0f;
		ci.Color[2] = 0xED / 255.0f;
		ci.Color[3] = 1.0f;
		ci.Depth = 1.0f;
		Clear(cb, &ci);
	}

	{
		SetShader(cb, g_TonemapShader);

		SetTexture(cb, 0, g_HdrColor, g_BasicSampler);

		SetVertexBuffers(cb, g_QuadSpec, &g_QuadVerts, 1);
		SetIndexBuffer(cb, g_QuadIndices, DataUInt16);
		DrawIndexed(cb, DrawTriangles, 6, 0);
	}
}

#endif
