#ifndef __BRIDGES_OPENGL_PIXEL_STORE_H
#define __BRIDGES_OPENGL_PIXEL_STORE_H

#include <stdbool.h>
#include <vitaGL.h>

void bridge_gl_pixel_store_i(GLenum name, GLint value);
bool bridge_gl_query_pixel_store(GLenum name, GLint* value);
void bridge_gl_reset_pixel_store(void);
void bridge_gl_tex_image_2d(GLenum target, GLint level, GLint internal_format, GLsizei width, GLsizei height,
    GLint border, GLenum format, GLenum type, const void* pixels);
void bridge_gl_tex_sub_image_2d(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, const void* pixels);

#endif
