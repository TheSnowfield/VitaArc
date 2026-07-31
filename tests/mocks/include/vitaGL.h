#ifndef __TESTS_MOCKS_VITAGL_H
#define __TESTS_MOCKS_VITAGL_H

#include <stddef.h>
#include <stdint.h>

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef char GLchar;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_OUT_OF_MEMORY 0x0505
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_FLOAT 0x1406
#define GL_ALPHA 0x1906
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190a
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_TEXTURE_2D 0x0de1
#define GL_UNPACK_ALIGNMENT 0x0cf5
#define GL_UNPACK_ROW_LENGTH 0x0cf2
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#define GL_FIXED 0x140c
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING 0x889f
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_VERTEX_ATTRIB_ARRAY_SIZE 0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE 0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED 0x886a
#define GL_VERTEX_ATTRIB_ARRAY_POINTER 0x8645
#define GL_COLOR_CLEAR_VALUE 0x0c22

void glGenBuffers(GLsizei count, GLuint* buffers);
void glBindBuffer(GLenum target, GLuint buffer);
void glDeleteBuffers(GLsizei count, const GLuint* buffers);
GLenum glGetError(void);
void glGetIntegerv(GLenum name, GLint* values);
void glGetFloatv(GLenum name, GLfloat* values);
void glGetBooleanv(GLenum name, GLboolean* values);
void glGetVertexAttribiv(GLuint index, GLenum name, GLint* values);
void glGetVertexAttribfv(GLuint index, GLenum name, GLfloat* values);
void glGetVertexAttribPointerv(GLuint index, GLenum name, void** pointer);
void glVertexAttribPointer(
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
void glGenVertexArrays(GLsizei count, GLuint* arrays);
void glBindVertexArray(GLuint array);
void glDeleteVertexArrays(GLsizei count, const GLuint* arrays);
void glPixelStorei(GLenum name, GLint value);
void glTexImage2D(GLenum target, GLint level, GLint internal_format, GLsizei width, GLsizei height, GLint border,
    GLenum format, GLenum type, const void* pixels);
void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
    GLenum type, const void* pixels);

#endif
