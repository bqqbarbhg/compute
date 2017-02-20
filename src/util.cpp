#include "util.h"
#include "renderer.h"
#include "opengl.h"
#include "../ext/stb_image.h"
#include <stdio.h>
#include <stdlib.h>

extern GLFWwindow *g_Window;

Shader *LoadVertFragShader(const char *path)
{
	char vs[128], fs[128];
	sprintf(vs, "%s_vert.glsl", path);
	sprintf(fs, "%s_frag.glsl", path);

	ShaderSource src[2];
	src[0].Type = ShaderTypeVertex;
	src[0].Source = ReadFile(vs, NULL);
	src[1].Type = ShaderTypeFragment;
	src[1].Source = ReadFile(fs, NULL);
	return CreateShader(src, 2);
}

Shader *LoadComputeShader(const char *path)
{
	char cs[128];
	sprintf(cs, "%s.glsl", path);

	ShaderSource src[1];
	src[0].Type = ShaderTypeCompute;
	src[0].Source = ReadFile(cs, NULL);
	return CreateShader(src, 1);
}

Texture *LoadImage(const char *path)
{
	int imageWidth, imageHeight;
	stbi_uc *image = stbi_load(path, &imageWidth, &imageHeight, 0, 4);

	Texture *texture = CreateStaticTexture2D((const void**)&image, 1, imageWidth, imageHeight, TexRGBA8);

	stbi_image_free(image);
	return texture;
}

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

void SetWindowTitle(const char *title)
{
	glfwSetWindowTitle(g_Window, title);
}

