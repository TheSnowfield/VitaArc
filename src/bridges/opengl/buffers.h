#ifndef __BRIDGES_OPENGL_BUFFERS_H
#define __BRIDGES_OPENGL_BUFFERS_H

#include <vitaGL.h>

void bridge_gl_gen_buffers(GLsizei count, GLuint* buffers);
void bridge_gl_bind_buffer(GLenum target, GLuint buffer);
void bridge_gl_delete_buffers(GLsizei count, const GLuint* buffers);
GLboolean bridge_gl_is_buffer(GLuint buffer);
GLenum bridge_gl_get_error(void);
void bridge_gl_set_error(GLenum error);
void bridge_gl_get_integer_v(GLenum name, GLint* values);
void bridge_gl_get_float_v(GLenum name, GLfloat* values);
void bridge_gl_get_boolean_v(GLenum name, GLboolean* values);
void bridge_gl_get_vertex_attrib_iv(GLuint index, GLenum name, GLint* values);
void bridge_gl_get_vertex_attrib_fv(GLuint index, GLenum name, GLfloat* values);
void bridge_gl_vertex_attrib_pointer(
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
void bridge_gl_gen_vertex_arrays(GLsizei count, GLuint* arrays);
void bridge_gl_bind_vertex_array(GLuint array);
void bridge_gl_delete_vertex_arrays(GLsizei count, const GLuint* arrays);
void bridge_gl_shutdown_buffers(void);

#endif
