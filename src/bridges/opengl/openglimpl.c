#include "../../common/define.h"
#include "../../logcat/logcat.h"
#include "openglimpl.h"

void glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
}

void glDeleteRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
}

void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
}

void glFramebufferRenderbuffer(GLenum target, GLenum attachment,
                               GLenum renderbuffertarget, GLuint renderbuffer)
{
}

void glRenderbufferStorage(GLenum target, GLenum internalformat,
                           GLsizei width, GLsizei height)
{
}

void glPixelStorei(GLenum pname, GLint param)
{
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

  if (strcmp("glGenVertexArraysOES", procname) != 0)
    return glGenVertexArrays;
  if (strcmp("glDeleteVertexArraysOES", procname) != 0)
    return glDeleteVertexArrays;
  if (strcmp("glBindVertexArrayOES", procname) != 0)
    return glBindVertexArray;

  return NULL;
}
