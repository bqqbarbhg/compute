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

	if (type != GL_DEBUG_TYPE_OTHER)
		__debugbreak();
}

void Initialize();
void Render();

int main(int argc, char **argv)
{
	if (!glfwInit())
	{
		fprintf(stderr, "glfwInit() failed\n");
		return 1;
	}

	glfwSetErrorCallback(&GlfwErrorCallback);

#if 0
	// Request OpenGL 4.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#else
#endif

	g_Window = glfwCreateWindow(1280, 720, "Compute", NULL, NULL);
	if (!g_Window)
	{
		fprintf(stderr, "glfwCreateWindow() failed\n");
		return 1;
	}

	glfwMakeContextCurrent(g_Window);

	// Load GL extensions
#if 0
	glewInit();

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(&GlDebugCallback, NULL);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

	glfwSwapInterval(1);

	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

	Device *dev = CreateDevice(g_Window);

	// Main loop
	while (!glfwWindowShouldClose(g_Window))
	{
		glfwPollEvents();

		BeginFrame(dev);
		CommandBuffer *cb = GetFrameCommandBuffer(dev);

		ClearInfo ci;
		ci.ClearColor = true;
		ci.ClearDepth = true;
		ci.ClearStencil = true;
		ci.Color[0] = 0x64 / 255.0f;
		ci.Color[1] = 0x95 / 255.0f;
		ci.Color[2] = 0xED / 255.0f;
		ci.Color[3] = 0.0f;

		Clear(cb, &ci);

		Present(dev);

		glfwSwapBuffers(g_Window);
	}

	glfwDestroyWindow(g_Window);
	glfwTerminate();
}


