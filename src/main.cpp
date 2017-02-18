#include "opengl.h"
#include <stdio.h>

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

void Render()
{
	glClearColor(0x64/255.0f, 0x95/255.0f, 0xED/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
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

