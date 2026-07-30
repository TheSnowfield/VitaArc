#include <stdlib.h>
#include <vitaGL.h>
#include <stdio.h>
#include <string.h>

#include <config.h>
#include "../../../logcat/logcat.h"
#include "../../../utils/fs.h"
#include "opengl.h"
#include "glsl2cg.h"

void bridge_gl_bind_renderbuffer(GLenum target, GLuint renderbuffer)
{
  (void)target;
  (void)renderbuffer;
  log_v(TAG, "Unsupported 'glBindRenderbuffer' called.");
}

void bridge_gl_delete_renderbuffers(GLsizei n, const GLuint *renderbuffers)
{
  (void)n;
  (void)renderbuffers;
  log_v(TAG, "Unsupported 'glDeleteRenderbuffers' called.");
}

void bridge_gl_gen_renderbuffers(GLsizei n, GLuint *renderbuffers)
{
  (void)n;
  (void)renderbuffers;
  log_v(TAG, "Unsupported 'glGenRenderbuffers' called.");
}

void bridge_gl_framebuffer_renderbuffer(GLenum target, GLenum attachment,
                                        GLenum renderbuffer_target,
                                        GLuint renderbuffer)
{
  (void)target;
  (void)attachment;
  (void)renderbuffer_target;
  (void)renderbuffer;
  log_v(TAG, "Unsupported 'glFramebufferRenderbuffer' called.");
}

void bridge_gl_renderbuffer_storage(GLenum target, GLenum internalformat,
                                    GLsizei width, GLsizei height)
{
  (void)target;
  (void)internalformat;
  (void)width;
  (void)height;
  log_v(TAG, "Unsupported 'glRenderbufferStorage' called.");
}

void bridge_gl_pixel_store_i(GLenum pname, GLint param)
{
  (void)pname;
  (void)param;
  log_v(TAG, "Unsupported 'glPixelStorei' called.");
}

typedef struct
{
  void *ptr;
  int32_t size;
  vglMemType type;
  GLboolean used;
} _gpubuffer;

GLboolean bridge_gl_is_buffer(GLuint buffer)
{
  log_v(TAG, "Called glIsBuffer(%d)", buffer);
  _gpubuffer *p = (_gpubuffer *)buffer;
  return (p != NULL);
}

GLboolean bridge_gl_is_renderbuffer(GLuint renderbuffer)
{
  log_v(TAG, "Called glIsRenderbuffer(%d)", renderbuffer);
  void *p = (void *)renderbuffer;
  return (p != NULL);
}

void *bridge_gl_map_buffer(GLenum target, GLenum access)
{
  (void)target;
  (void)access;
  log_v(TAG, "Unsupported 'glMapBuffer' called.");
  return NULL;
}

GLboolean bridge_gl_unmap_buffer(GLenum target)
{
  (void)target;
  log_v(TAG, "Unsupported 'glUnmapBuffer' called.");
  return GL_FALSE;
}

void bridge_gl_gen_vertex_arrays(GLsizei n, GLuint *arrays)
{
  (void)n;
  (void)arrays;
  log_v(TAG, "Unsupported 'glGenVertexArrays' called.");
}

void bridge_gl_delete_vertex_arrays(GLsizei n, const GLuint *arrays)
{
  (void)n;
  (void)arrays;
  log_v(TAG, "Unsupported 'glDeleteVertexArrays' called.");
}

void bridge_gl_bind_vertex_array(GLuint array)
{
  log_v(TAG, "Unsupported 'glBindVertexArray' called.");
}

void (*bridge_egl_get_proc_address(char const *proc_name))(void)
{
  log_v(TAG, "eglGetProcAddress(\"%s\")", proc_name);

  if (strcmp("glGenVertexArraysOES", proc_name) == 0)
    return (void (*)(void))bridge_gl_gen_vertex_arrays;
  if (strcmp("glDeleteVertexArraysOES", proc_name) == 0)
    return (void (*)(void))bridge_gl_delete_vertex_arrays;
  if (strcmp("glBindVertexArrayOES", proc_name) == 0)
    return (void (*)(void))bridge_gl_bind_vertex_array;

  return NULL;
}

void _gl_shader_source(GLuint handle, GLsizei count,
                      const GLchar *const *string, const GLint *length)
{
  (void)length;

  int type;
  char *cg_shader;
  char *gl_shader, *gl_shader_pos;

  // calculate shader length
  size_t size = 0;
  for(size_t i = 0; i < count; ++i) {
    size += strlen(string[i]);
  }

  log_i("opengl.c", "Accepted shader source: %d shaders, totoal size %d", count, size);

  // combine the string array
  gl_shader = malloc(size + 1);
  gl_shader_pos = gl_shader;
  for(size_t i = 0; i < count; ++i) {
    memcpy(gl_shader_pos, string[i], strlen(string[i]));
    gl_shader_pos += strlen(string[i]);
  }

  // sealed the string
  gl_shader[size] = '\0';

  log_printf("====\n");
  log_printf("%s", gl_shader);
  log_printf("\n====\n");

  FILE *shader_file = fopen("ux0:vitaarc/shader/1.glsl", "w+");
  fwrite(gl_shader, size, 1, shader_file);
  fclose(shader_file);

  // convert glsl shader to cg
  glGetShaderiv(handle, GL_SHADER_TYPE, &type); {
    if (type == GL_FRAGMENT_SHADER) {
      cg_shader = translate_frag_shader(gl_shader, size);
    } else {
      cg_shader = translate_vert_shader(gl_shader, size);
    }
  }
  log_printf("====\n");
  log_printf("%s", cg_shader);
  log_printf("====\n");

  // create shader source
  const GLchar *cg_shader_source = cg_shader;
  glShaderSource(handle, 1, &cg_shader_source, NULL);
  free(cg_shader);
  free(gl_shader);
}

// void _gl_get_shader_iv(GLuint handle, GLenum pname, GLint *params)
// {
//   log_v(TAG, "called _gl_get_shader_iv(%d, %d, %08X), *params = %d", handle, pname, params, *params);

//   glGetShaderiv(handle, pname, params);
  
//   if(pname == GL_COMPILE_STATUS)
//     log_v(TAG, "compile result %d", *params);
// }


void _gl_get_shader_source(GLuint shader, GLsizei buffer_size,
                           GLsizei *length, GLchar *source) {
  (void)shader;
  (void)buffer_size;
  (void)length;
  (void)source;

}

void _gl_bind_attrib_location(GLuint program, GLuint index, const GLchar *name) {
  log_i(TAG, "_gl_bind_attrib_location(%d, %d, %s)", program, index, name);
}
