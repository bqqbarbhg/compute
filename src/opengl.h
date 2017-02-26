#pragma once

#ifdef RENDERER_VULKAN
	#define GLFW_INCLUDE_VULKAN
#endif

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>

