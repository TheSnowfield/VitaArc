#ifndef __BRIDGES_OPENGL_IMPL_OPENGL_H
#define __BRIDGES_OPENGL_IMPL_OPENGL_H

#include <vitaGL.h>

void bridge_gl_bind_renderbuffer(GLenum target, GLuint renderbuffer);

void bridge_gl_delete_renderbuffers(GLsizei n, const GLuint *renderbuffers);

void bridge_gl_gen_renderbuffers(GLsizei n, GLuint *renderbuffers);

void bridge_gl_framebuffer_renderbuffer(GLenum target, GLenum attachment,
                                        GLenum renderbuffer_target,
                                        GLuint renderbuffer);

void bridge_gl_renderbuffer_storage(GLenum target, GLenum internalformat,
                                 GLsizei width, GLsizei height);

void bridge_gl_pixel_store_i(GLenum pname, GLint param);

void (*bridge_egl_get_proc_address(char const *proc_name))(void);

GLboolean bridge_gl_is_buffer(GLuint buffer);

GLboolean bridge_gl_is_renderbuffer(GLuint renderbuffer);

void bridge_gl_gen_vertex_arrays(GLsizei n, GLuint *arrays);

void bridge_gl_delete_vertex_arrays(GLsizei n, const GLuint *arrays);

void bridge_gl_bind_vertex_array(GLuint array);

void *bridge_gl_map_buffer(GLenum target, GLenum access);

GLboolean bridge_gl_unmap_buffer(GLenum target);

void _gl_shader_source(GLuint handle, GLsizei count,
                     const GLchar *const *string, const GLint *length);

void _gl_get_shader_iv(GLuint handle, GLenum pname, GLint *params);

void _gl_get_shader_source(GLuint shader, GLsizei buffer_size,
                           GLsizei *length, GLchar *source);

void _gl_bind_attrib_location(GLuint program, GLuint index, const GLchar *name);

#endif /* __BRIDGES_OPENGL_IMPL_OPENGL_H */
