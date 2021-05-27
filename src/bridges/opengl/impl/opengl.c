#include <vitaGL.h>

#include "../../../common/define.h"
#include "../../../logcat/logcat.h"
#include "opengl.h"

void glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
  logV(TAG, "Unsupported 'glBindRenderbuffer' called.");
}

void glDeleteRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  logV(TAG, "Unsupported 'glDeleteRenderbuffers' called.");
}

void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  logV(TAG, "Unsupported 'glGenRenderbuffers' called.");
}

void glFramebufferRenderbuffer(GLenum target, GLenum attachment,
                               GLenum renderbuffertarget, GLuint renderbuffer)
{
  logV(TAG, "Unsupported 'glFramebufferRenderbuffer' called.");
}

void glRenderbufferStorage(GLenum target, GLenum internalformat,
                           GLsizei width, GLsizei height)
{
  logV(TAG, "Unsupported 'glRenderbufferStorage' called.");
}

void glPixelStorei(GLenum pname, GLint param)
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

GLboolean glIsBuffer(GLuint buffer)
{
  logV(TAG, "Called glIsBuffer(%d)", buffer);
  _gpubuffer *p = (_gpubuffer *)buffer;
  return (p != NULL);
}

GLboolean glIsRenderbuffer(GLuint renderbuffer)
{
  logV(TAG, "Called glIsRenderbuffer(%d)", renderbuffer);
  void *p = (void *)renderbuffer;
  return (p != NULL);
}

void *glMapBuffer(GLenum target, GLenum access)
{
  logV(TAG, "Unsupported 'glMapBuffer' called.");
  return NULL;
}

GLboolean glUnmapBuffer(GLenum target)
{
  logV(TAG, "Unsupported 'glUnmapBuffer' called.");
  return GL_FALSE;
}

void glGenVertexArrays(GLsizei n, GLuint *arrays)
{
  logV(TAG, "Unsupported 'glGenVertexArrays' called.");
}

void glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
  logV(TAG, "Unsupported 'glDeleteVertexArrays' called.");
}

void glBindVertexArray(GLuint array)
{
  logV(TAG, "Unsupported 'glBindVertexArray' called.");
}

void *eglGetProcAddress(char const *procname)
{
  logV(TAG, "eglGetProcAddress(\"%s\")", procname);

  if (strcmp("glGenVertexArraysOES", procname) == 0)
    return glGenVertexArrays;
  if (strcmp("glDeleteVertexArraysOES", procname) == 0)
    return glDeleteVertexArrays;
  if (strcmp("glBindVertexArrayOES", procname) == 0)
    return glBindVertexArray;

  return NULL;
}
