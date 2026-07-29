#ifndef _BRIDGE_OPENGL_IMPL_H_
#define _BRIDGE_OPENGL_IMPL_H_

#include <vitaGL.h>

void bridgeGlBindRenderbuffer(GLenum target, GLuint renderbuffer);

void bridgeGlDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers);

void bridgeGlGenRenderbuffers(GLsizei n, GLuint *renderbuffers);

void bridgeGlFramebufferRenderbuffer(GLenum target, GLenum attachment,
                                     GLenum renderbuffertarget, GLuint renderbuffer);

void bridgeGlRenderbufferStorage(GLenum target, GLenum internalformat,
                                 GLsizei width, GLsizei height);

void bridgeGlPixelStorei(GLenum pname, GLint param);

void (*bridgeEglGetProcAddress(char const *procname))(void);

GLboolean bridgeGlIsBuffer(GLuint buffer);

GLboolean bridgeGlIsRenderbuffer(GLuint renderbuffer);

void bridgeGlGenVertexArrays(GLsizei n, GLuint *arrays);

void bridgeGlDeleteVertexArrays(GLsizei n, const GLuint *arrays);

void bridgeGlBindVertexArray(GLuint array);

void *bridgeGlMapBuffer(GLenum target, GLenum access);

GLboolean bridgeGlUnmapBuffer(GLenum target);

void _glShaderSource(GLuint handle, GLsizei count,
                     const GLchar *const *string, const GLint *length);

void _glGetShaderiv(GLuint handle, GLenum pname, GLint *params);

void _glGetShaderSource(	GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);

void _glBindAttribLocation(GLuint program, GLuint index, const GLchar *name);

#endif /* _BRIDGE_OPENGL_IMPL_H_ */
