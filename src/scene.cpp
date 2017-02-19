#include "renderer.h"
#include "../ext/stb_image.h"
#include "../ext/tinyobj_loader.h"
#include <vector>
#include <unordered_map>
#include "math.h"

#define ArrayCount(arr) (sizeof(arr) / sizeof(*(arr)))

CommandBuffer *g_CommandBuffer;
VertexSpec *g_ObjSpec;
Sampler *g_ObjSampler;
Shader *g_ObjShader;

struct Object
{
	Buffer *VertexBuffer;
	Buffer *IndexBuffer;
	Texture *Texture;
	uint32_t NumIndices;
};

Object g_Objects[32];
uint32_t g_NumObjects;

Buffer *g_UniformBuffers[32];
uint32_t g_UniformIndex;

struct ObjVertex
{
	float x, y, z;
	float u, v;
};

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
};

char *ReadFile(const char *path, size_t *pSize)
{
	FILE *file = fopen(path, "rb");
	if (!file) return NULL;

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (pSize)
		*pSize = size;

	char *buf = (char*)malloc(size + 1);
	fread(buf, 1, size, file);

	buf[size] = '\0';
	fclose(file);

	return buf;
}

void Initialize()
{
	{
		ShaderSource src[2];
		src[0].Type = ShaderTypeVertex;
		src[0].Source = ReadFile("shader/object/object_vert.glsl", NULL);
		src[1].Type = ShaderTypeFragment;
		src[1].Source = ReadFile("shader/object/object_frag.glsl", NULL);
		g_ObjShader = CreateShader(src, 2);
	}

	{
		SamplerInfo si;
		si.Min = si.Mag = si.Mip = FilterLinear;
		si.WrapU = si.WrapV = si.WrapW = WrapWrap;
		si.Anisotropy = 0;
		g_ObjSampler = CreateSampler(&si);
	}

	g_ObjSpec = CreateVertexSpec(ObjVertex_Elements, ArrayCount(ObjVertex_Elements));
	g_CommandBuffer = CreateCommandBuffer();

	for (uint32_t i = 0; i < ArrayCount(g_UniformBuffers); i++)
		g_UniformBuffers[i] = CreateBuffer(BufferUniform);

	{
		int imageWidth, imageHeight;
		stbi_uc *image = stbi_load("mesh/houses.png", &imageWidth, &imageHeight, 0, 4);

		Texture *texture = CreateStaticTexture2D((const void**)&image, 1, imageWidth, imageHeight, TexRGBA8);

		std::string inputfile = "mesh/houses.obj";
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
				assert(shape->mesh.num_face_vertices[faceI] == 3);
				for (uint32_t i = 0; i < 3; i++)
				{
					tinyobj::index_t idx = shape->mesh.indices[indexOffset + i];

					float *vv = &attrib.vertices[idx.vertex_index * 3];
					float *vt = &attrib.texcoords[idx.texcoord_index * 2];

					ObjVertex v;
					v.x = vv[0];
					v.y = vv[1];
					v.z = vv[2];
					v.u = vt[0];
					v.v = 1.0f - vt[1];

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
			}

			Object *obj = &g_Objects[shapeI];
			obj->VertexBuffer = CreateStaticBuffer(BufferVertex,
					vertices.data(), vertices.size() * sizeof(ObjVertex));
			obj->IndexBuffer = CreateStaticBuffer(BufferIndex,
					indices.data(), indices.size() * sizeof(uint16_t));
			obj->Texture = texture;

			obj->NumIndices = (uint32_t)indices.size();
		}

	}
}

void PushUniform(CommandBuffer *cb, uint32_t index, const void *data, uint32_t size)
{
	uint32_t ix = g_UniformIndex;
	g_UniformIndex = (g_UniformIndex + 1) % ArrayCount(g_UniformBuffers);

	Buffer *b = g_UniformBuffers[g_UniformIndex];
	SetBufferData(b, data, size);
	
	SetUniformBuffer(cb, 0, b);
}

struct ObjectUniform
{
	Mat44 u_WorldViewProjection;
};

void Render()
{
	CommandBuffer *cb = g_CommandBuffer;

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

	static float TTT;
	TTT += 0.016f;

	Mat44 proj = mat44_perspective(1.5f, 1280.0f/720.0f, 0.1f, 1000.0f);
	Mat44 view = mat44_lookat(
			vec3(sinf(TTT) * 5.0f, 4.0f, cosf(TTT) * 5.0f),
			vec3(0.0f, 0.0f, 0.0f),
			vec3(0.0f, 1.0f, 0.0f));

	
	SetShader(cb, g_ObjShader);
	
	ObjectUniform ou;
	ou.u_WorldViewProjection = transpose(view * proj);
	PushUniform(cb, 0, &ou, sizeof(ou));

	for (uint32_t i = 0; i < g_NumObjects; i++)
	{
		Object *obj = &g_Objects[i];

		SetVertexBuffers(cb, g_ObjSpec, &obj->VertexBuffer, 1);
		SetIndexBuffer(cb, obj->IndexBuffer, DataUInt16);
		SetTexture(cb, 0, obj->Texture, g_ObjSampler);
		DrawIndexed(cb, DrawTriangles, obj->NumIndices, 0);
	}
}
