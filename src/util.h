#pragma once

struct Texture;
struct Shader;

#define ArrayCount(arr) (sizeof(arr) / sizeof(*(arr)))

Shader *LoadVertFragShader(const char *path);
Shader *LoadComputeShader(const char *path);
Texture *LoadImage(const char *path);
char *ReadFile(const char *path, size_t *pSize);
void SetWindowTitle(const char *title);

