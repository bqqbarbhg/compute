#include "opengl.h"
#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>

#define ArrayCount(arr) (sizeof(arr) / sizeof(*(arr)))

GLFWwindow *g_Window;

void GlfwErrorCallback(int error, const char *description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void APIENTRY GlDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
		GLsizei length, const GLchar* message, const void *userParam)
{
	const char *mType = "Message";
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR: mType = "Error"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: mType = "Deprecated behavior"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: mType = "Undefined behavior"; break;
	case GL_DEBUG_TYPE_PORTABILITY: mType = "Portability"; break;
	case GL_DEBUG_TYPE_PERFORMANCE: mType = "Performance"; break;
	}

	fprintf(stderr, "GL %s: %s\n", mType, (const char*)message);

	__debugbreak();
}

Shader *ObjectShader;

char *ReadFile(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file) return NULL;

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *buf = (char*)malloc(size + 1);
	fread(buf, 1, size, file);

	buf[size] = '\0';
	fclose(file);

	return buf;
}

Buffer *VB[2], *UniformBuffer, *IB;
VertexSpec *VSpec;
CommandBuffer *CBuf;
Texture *Tex;
Sampler *Samp;

struct VPos3D
{
	float x, y, z;
};

struct VCol
{
	uint32_t col;
};

VertexElement VPos3D_Col_Elements[] = {
	{ 0, 0, 3, DataFloat, false, 0*4, 3*4 },
	{ 1, 1, 4, DataUInt8, true, 0*4, 1*4 },
};

VPos3D TrianglePos[] = {
	{ -1.0f, +1.0f, 0.0f },
	{ +1.0f, +1.0f, 0.0f },
	{ -1.0f, -1.0f, 0.0f },
	{ +1.0f, -1.0f, 0.0f },
};

VCol TriangleCol[] = {
	0xFF0000FF,
	0xFF00FF00,
	0xFFFF0000,
	0xFFFF00FF,
};

uint32_t TexData[] = {
	0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFF00FF,
	0xFFFF0000, 0xFFFF00FF, 0xFF0000FF, 0xFF00FF00,
	0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFF00FF,
	0xFFFF0000, 0xFFFF00FF, 0xFF0000FF, 0xFF00FF00,
};

uint16_t TriangleIndex[] = {
	0, 1, 2, 1, 2, 3,
};

float ModelViewProjection[16] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f,
};

void Initialize()
{
	{
		ShaderSource src[2];
		src[0].Type = ShaderTypeVertex;
		src[0].Source = ReadFile("shader/object/object_vert.glsl");
		src[1].Type = ShaderTypeFragment;
		src[1].Source = ReadFile("shader/object/object_frag.glsl");
		ObjectShader = CreateShader(src, 2);
	}

	{
		SamplerInfo si;
		si.Min = si.Mag = si.Mip = FilterLinear;
		si.WrapU = si.WrapV = si.WrapW = WrapWrap;
		si.Anisotropy = 0;
		Samp = CreateSampler(&si);
	}

	CBuf = CreateCommandBuffer();

	VSpec = CreateVertexSpec(VPos3D_Col_Elements, ArrayCount(VPos3D_Col_Elements));
	UniformBuffer = CreateStaticBuffer(BufferUniform, ModelViewProjection, sizeof(ModelViewProjection));
	VB[0] = CreateStaticBuffer(BufferVertex, TrianglePos, sizeof(TrianglePos));
	VB[1] = CreateStaticBuffer(BufferVertex, TriangleCol, sizeof(TriangleCol));
	IB = CreateStaticBuffer(BufferIndex, TriangleIndex, sizeof(TriangleIndex));

	const void *data = TexData;
	Tex = CreateStaticTexture2D(&data, 1, 4, 4, TexRGBA8);
}

void Render()
{
	glClearColor(0x64/255.0f, 0x95/255.0f, 0xED/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	CommandBuffer *cb = CBuf;

	SetUniformBuffer(cb, 0, UniformBuffer);
	SetVertexBuffers(cb, VSpec, VB, 2);
	SetIndexBuffer(cb, IB, DataUInt16);
	SetTexture(cb, 0, Tex, Samp);
	SetShader(cb, ObjectShader);
	DrawIndexed(cb, DrawTriangles, 6, 0);
}

int main(int argc, char **argv)
{
	if (!glfwInit())
	{
		fprintf(stderr, "glfwInit() failed\n");
		return 1;
	}

	glfwSetErrorCallback(&GlfwErrorCallback);

	// Request OpenGL 4.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

	g_Window = glfwCreateWindow(1280, 720, "Compute", NULL, NULL);
	if (!g_Window)
	{
		fprintf(stderr, "glfwCreateWindow() failed\n");
		return 1;
	}

	glfwMakeContextCurrent(g_Window);

	// Load GL extensions
	glewInit();

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(&GlDebugCallback, NULL);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

	Initialize();

	// Main loop
	while (!glfwWindowShouldClose(g_Window))
	{
		glfwPollEvents();

		Render();

		glfwSwapBuffers(g_Window);
	}

	glfwDestroyWindow(g_Window);
	glfwTerminate();
}

