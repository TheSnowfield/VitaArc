#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
#include <algorithm>
using std::min;
#else
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef unsigned GLenum;
typedef int GLsizei;
typedef int GLboolean;
typedef unsigned vglSemanticType;
#define GL_TRUE 1
#define GL_FALSE 0
#define GL_VERTEX_SHADER 0x8b31
#define GL_FRAGMENT_SHADER 0x8b30
#define MAX_CG_TEXCOORD_ID 10
#define MAX_CG_COLOR_ID 2
#define VGL_TYPE_TEXCOORD 0
#define VGL_TYPE_TEXCOORD_CENTROID 1
#define VGL_TYPE_COLOR 2
#define VGL_TYPE_COLOR_CENTROID 3
#define VGL_TYPE_FOG 4
#define VGL_TYPE_FOG_CENTROID 5
#define VGL_TYPE_CLIP 6
#define VGL_MODE_SHADER_PAIR 0
#define VGL_MODE_GLOBAL 1
#define VGL_MODE_POSTPONED 2
#define vglMalloc malloc
#define vgl_realloc realloc
#define vgl_free free
#define vgl_fast_memcpy memcpy
#define vgl_memset memset
#define sceClibMemcpy memcpy
#define sceClibMemmove memmove
#define vgl_log(...) fprintf(stderr, __VA_ARGS__)

typedef struct {
    char texcoord_names[10][64];
    char color_names[2][64];
    GLboolean texcoord_used[10];
    GLboolean color_used[2];
} binds_map;

typedef struct {
    GLenum type;
    char* source;
    binds_map semantics;
    void* prog;
    uint32_t size;
    int is_glsl;
} shader;

#ifdef __cplusplus
extern "C" {
#endif

void vglAddSemanticBinding(const char* name, int index, GLenum type);

#ifdef __cplusplus
}
#endif
