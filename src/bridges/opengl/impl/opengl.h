#ifndef _BRIDGE_OPENGL_IMPL_H_
#define _BRIDGE_OPENGL_IMPL_H_

#include <vitaGL.h>

void glBindRenderbuffer(GLenum target, GLuint renderbuffer);

void glDeleteRenderbuffers(GLsizei n, GLuint *renderbuffers);

void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers);

void glFramebufferRenderbuffer(GLenum target, GLenum attachment,
                               GLenum renderbuffertarget, GLuint renderbuffer);

void glRenderbufferStorage(GLenum target, GLenum internalformat,
                           GLsizei width, GLsizei height);

void glPixelStorei(GLenum pname, GLint param);

void *eglGetProcAddress(char const *procname);

GLboolean glIsBuffer(GLuint buffer);

GLboolean glIsRenderbuffer(GLuint renderbuffer);

void glGenVertexArrays(GLsizei n, GLuint *arrays);

void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);

void glBindVertexArray(GLuint array);

void *glMapBuffer(GLenum target, GLenum access);

GLboolean glUnmapBuffer(GLenum target);

#endif /* _BRIDGE_OPENGL_IMPL_H_ */
