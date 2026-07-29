#include <stdlib.h>
#include <vitaGL.h>
#include <stdio.h>
#include <string.h>

#include <define.h>
#include "../../../logcat/logcat.h"
#include "../../../utils/fs.h"
#include "opengl.h"
#include "glsl2cg.h"

void bridgeGlBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
  logV(TAG, "Unsupported 'glBindRenderbuffer' called.");
}

void bridgeGlDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
  logV(TAG, "Unsupported 'glDeleteRenderbuffers' called.");
}

void bridgeGlGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  logV(TAG, "Unsupported 'glGenRenderbuffers' called.");
}

void bridgeGlFramebufferRenderbuffer(GLenum target, GLenum attachment,
                                     GLenum renderbuffertarget, GLuint renderbuffer)
{
  logV(TAG, "Unsupported 'glFramebufferRenderbuffer' called.");
}

void bridgeGlRenderbufferStorage(GLenum target, GLenum internalformat,
                                 GLsizei width, GLsizei height)
{
  logV(TAG, "Unsupported 'glRenderbufferStorage' called.");
}

void bridgeGlPixelStorei(GLenum pname, GLint param)
{
  logV(TAG, "Unsupported 'glPixelStorei' called.");
}

typedef struct
{
  void *ptr;
  int32_t size;
  vglMemType type;
  GLboolean used;
} _gpubuffer;

GLboolean bridgeGlIsBuffer(GLuint buffer)
{
  logV(TAG, "Called glIsBuffer(%d)", buffer);
  _gpubuffer *p = (_gpubuffer *)buffer;
  return (p != NULL);
}

GLboolean bridgeGlIsRenderbuffer(GLuint renderbuffer)
{
  logV(TAG, "Called glIsRenderbuffer(%d)", renderbuffer);
  void *p = (void *)renderbuffer;
  return (p != NULL);
}

void *bridgeGlMapBuffer(GLenum target, GLenum access)
{
  logV(TAG, "Unsupported 'glMapBuffer' called.");
  return NULL;
}

GLboolean bridgeGlUnmapBuffer(GLenum target)
{
  logV(TAG, "Unsupported 'glUnmapBuffer' called.");
  return GL_FALSE;
}

void bridgeGlGenVertexArrays(GLsizei n, GLuint *arrays)
{
  logV(TAG, "Unsupported 'glGenVertexArrays' called.");
}

void bridgeGlDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
  logV(TAG, "Unsupported 'glDeleteVertexArrays' called.");
}

void bridgeGlBindVertexArray(GLuint array)
{
  logV(TAG, "Unsupported 'glBindVertexArray' called.");
}

void (*bridgeEglGetProcAddress(char const *procname))(void)
{
  logV(TAG, "eglGetProcAddress(\"%s\")", procname);

  if (strcmp("glGenVertexArraysOES", procname) == 0)
    return (void (*)(void))bridgeGlGenVertexArrays;
  if (strcmp("glDeleteVertexArraysOES", procname) == 0)
    return (void (*)(void))bridgeGlDeleteVertexArrays;
  if (strcmp("glBindVertexArrayOES", procname) == 0)
    return (void (*)(void))bridgeGlBindVertexArray;

  return NULL;
}

void _glShaderSource(GLuint handle, GLsizei count,
                     const GLchar *const *string, const GLint *length)
{

  int type;
  char *cg_shader;
  char *gl_shader, *gl_shader_pos;

  // calculate shader length
  size_t size = 0;
  for(size_t i = 0; i < count; ++i) {
    size += strlen(string[i]);
  }

  logI("opengl.c", "Accepted shader source: %d shaders, totoal size %d", count, size);

  // combine the string array
  gl_shader = malloc(size + 1);
  gl_shader_pos = gl_shader;
  for(size_t i = 0; i < count; ++i) {
    memcpy(gl_shader_pos, string[i], strlen(string[i]));
    gl_shader_pos += strlen(string[i]);
  }

  // sealed the string
  gl_shader[size] = '\0';

  logPrintf("====\n");
  logPrintf("%s", gl_shader);
  logPrintf("\n====\n");

  FILE *fd = fopen("ux0:vitaarc/shader/1.glsl", "w+");
  fwrite(gl_shader, size, 1, fd);
  fclose(fd);

  // convert glsl shader to cg
  glGetShaderiv(handle, GL_SHADER_TYPE, &type); {
    if (type == GL_FRAGMENT_SHADER) {
      cg_shader = translate_frag_shader(gl_shader, size);
    } else {
      cg_shader = translate_vert_shader(gl_shader, size);
    }
  }
  logPrintf("====\n");
  logPrintf("%s", cg_shader);
  logPrintf("====\n");

  // create shader source
  const GLchar *cg_shader_source = cg_shader;
  glShaderSource(handle, 1, &cg_shader_source, NULL);
  free(cg_shader);
  free(gl_shader);
}

// void _glGetShaderiv(GLuint handle, GLenum pname, GLint *params)
// {
//   logV(TAG, "called _glGetShaderiv(%d, %d, %08X), *params = %d", handle, pname, params, *params);

//   glGetShaderiv(handle, pname, params);
  
//   if(pname == GL_COMPILE_STATUS)
//     logV(TAG, "compile result %d", *params);
// }


void _glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source) {

}

void _glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) {
  logI(TAG, "_glBindAttribLocation(%d, %d, %s)", program, index, name);
}
