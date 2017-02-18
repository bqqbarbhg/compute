#include "opengl.h"
#include "shader.h"
#include <stdio.h>
#include <stdlib.h>

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

GLuint VertexBuffer;
GLuint VertexSpec;
GLuint UniformBuffer;

struct VPos3DCol
{
	float x, y, z;
	uint32_t col;
};

VPos3DCol Triangle[] = {
	{ 0.0f, +1.0f, 0.0f, 0xFF0000FF },
	{ -1.0f, 0.0f, 0.0f, 0xFF00FF00 },
	{ +1.0f, 0.0f, 0.0f, 0xFFFF0000 },
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
		glGenBuffers(1, &VertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Triangle), Triangle, GL_STATIC_DRAW);
	}

	{
		glGenVertexArrays(1, &VertexSpec);
		glBindVertexArray(VertexSpec);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * 4, (const GLvoid*)(0 * 4));
		glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, 4 * 4, (const GLvoid*)(3 * 4));
	}

	{
		glGenBuffers(1, &UniformBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, UniformBuffer);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(ModelViewProjection), ModelViewProjection, GL_STATIC_DRAW);
	}
}

void Render()
{
	glClearColor(0x64/255.0f, 0x95/255.0f, 0xED/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, UniformBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
	glBindVertexArray(VertexSpec);

	SetShader(ObjectShader);

	glDrawArrays(GL_TRIANGLES, 0, 3);
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

