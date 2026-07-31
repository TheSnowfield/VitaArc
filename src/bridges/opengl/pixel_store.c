#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pixel_store.h"
#include "buffers.h"

/* The installed vitaGL accepts UNPACK_ROW_LENGTH but always consumes tightly
 * packed rows. Preserve GLES alignment semantics in the upload adapters. */
static GLint __unpack_alignment = 4;
static GLint __unpack_row_length;

void bridge_gl_pixel_store_i(GLenum name, GLint value)
{
    switch(name) {
        case GL_UNPACK_ALIGNMENT:
            if(value != 1 && value != 2 && value != 4 && value != 8) {
                bridge_gl_set_error(GL_INVALID_VALUE);
                return;
            }

            __unpack_alignment = value;
            return;

        case GL_UNPACK_ROW_LENGTH:
            if(value < 0) {
                bridge_gl_set_error(GL_INVALID_VALUE);
                return;
            }

            __unpack_row_length = value;
            break;
    }

    glPixelStorei(name, value);
}

bool bridge_gl_query_pixel_store(GLenum name, GLint* value)
{
    GLint result;
    switch(name) {
        case GL_UNPACK_ALIGNMENT:
            result = __unpack_alignment;
            break;

        case GL_UNPACK_ROW_LENGTH:
            result = __unpack_row_length;
            break;

        default: return false;
    }

    if(value) {
        *value = result;
    }

    return true;
}

void bridge_gl_reset_pixel_store(void)
{
    __unpack_alignment = 4;
    __unpack_row_length = 0;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

static size_t __pixel_size(GLenum format, GLenum type)
{
    if(type == GL_UNSIGNED_SHORT_5_6_5 && format == GL_RGB) {
        return 2;
    }

    if((type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_5_5_5_1) && format == GL_RGBA) {
        return 2;
    }

    size_t components;
    switch(format) {
        case GL_ALPHA:
        case GL_LUMINANCE:
            components = 1;
            break;

        case GL_LUMINANCE_ALPHA:
            components = 2;
            break;

        case GL_RGB:
            components = 3;
            break;

        case GL_RGBA:
            components = 4;
            break;

        default: return 0;
    }

    switch(type) {
        case GL_UNSIGNED_BYTE: return components;
        case GL_FLOAT:         return components * 4;
        default:               return 0;
    }
}

static bool __unpack(GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels, const void** source,
    void** storage, bool* reset_rows)
{
    *source = pixels;
    *storage = NULL;
    *reset_rows = false;
    /* Let vitaGL validate formats, empty images and GXM's dimension limit before
     * touching client memory. Null data also requests allocation without upload. */
    size_t pixel_size = __pixel_size(format, type);
    if(!pixels || !pixel_size || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        return true;
    }

    *reset_rows = __unpack_row_length != 0;
    size_t row_pixels = __unpack_row_length ? (size_t)__unpack_row_length : (size_t)width;
    if(row_pixels > (SIZE_MAX - 7) / pixel_size) {
        bridge_gl_set_error(GL_INVALID_VALUE);
        return false;
    }

    size_t row_bytes = (size_t)width * pixel_size;
    size_t stride = (row_pixels * pixel_size + __unpack_alignment - 1) & ~((size_t)__unpack_alignment - 1);
    if((size_t)height > SIZE_MAX / row_bytes ||
        ((size_t)height > 1 && stride > (SIZE_MAX - row_bytes) / ((size_t)height - 1))) {
        bridge_gl_set_error(GL_INVALID_VALUE);
        return false;
    }

    if(stride == row_bytes) {
        return true;
    }

    unsigned char* packed = malloc(row_bytes * (size_t)height);
    if(!packed) {
        bridge_gl_set_error(GL_OUT_OF_MEMORY);
        return false;
    }

    for(GLsizei row = 0; row < height; ++row) {
        memcpy(packed + (size_t)row * row_bytes, (const unsigned char*)pixels + (size_t)row * stride, row_bytes);
    }

    *source = packed;
    *storage = packed;
    return true;
}

void bridge_gl_tex_image_2d(GLenum target, GLint level, GLint internal_format, GLsizei width, GLsizei height,
    GLint border, GLenum format, GLenum type, const void* pixels)
{
    const void* source;
    void* storage;
    bool reset_rows;
    if(!__unpack(width, height, format, type, pixels, &source, &storage, &reset_rows)) {
        return;
    }

    if(reset_rows) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    glTexImage2D(target, level, internal_format, width, height, border, format, type, source);
    if(reset_rows) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, __unpack_row_length);
    }

    free(storage);
}

void bridge_gl_tex_sub_image_2d(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, const void* pixels)
{
    const void* source;
    void* storage;
    bool reset_rows;
    if(!__unpack(width, height, format, type, pixels, &source, &storage, &reset_rows)) {
        return;
    }

    if(reset_rows) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    glTexSubImage2D(target, level, x, y, width, height, format, type, source);
    if(reset_rows) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, __unpack_row_length);
    }

    free(storage);
}
