#pragma once

struct Texture;
struct Shader;

#define ArrayCount(arr) (sizeof(arr) / sizeof(*(arr)))

Shader *LoadVertFragShader(const char *path);
Shader *LoadComputeShader(const char *path);
Texture *LoadImage(const char *path);
char *ReadFile(const char *path, size_t *pSize);
void SetWindowTitle(const char *title);

uint64_t BeginMeasureCpuTime();
double EndMeasureCpuTime(uint64_t begin);

