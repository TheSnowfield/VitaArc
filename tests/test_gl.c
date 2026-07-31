#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <opengl/gl.h>

#define MAX_NATIVE_OBJECTS 256
#define ATTRIB_COUNT 8

typedef struct mock_attrib {
    GLuint buffer;
    GLint size, type, normalized, stride;
    const void* pointer;
} mock_attrib_t;

typedef struct mock_array {
    bool alive;
    GLuint element;
    mock_attrib_t attribs[ATTRIB_COUNT];
} mock_array_t;

static bool __native_buffers[MAX_NATIVE_OBJECTS];
static mock_array_t __native_arrays[MAX_NATIVE_OBJECTS] = {{.alive = true}};

static GLuint __native_array_buffer;
static GLuint __native_current_array;
static GLuint __next_native_buffer = 1;
static GLuint __next_native_array = 1;
static unsigned __live_buffers, __live_arrays;
static GLenum __native_error;
static bool __fail_native_buffer, __fail_native_array;
static bool __fail_native_attrib;
static int __allocations_before_failure = -1;

void* __real_calloc(size_t count, size_t size);
void* __real_malloc(size_t size);

void* __wrap_malloc(size_t size)
{
    if(__allocations_before_failure == 0) {
        return NULL;
    }

    if(__allocations_before_failure > 0) {
        --__allocations_before_failure;
    }

    return __real_malloc(size);
}

void* __wrap_calloc(size_t count, size_t size)
{
    if(__allocations_before_failure == 0) {
        return NULL;
    }

    if(__allocations_before_failure > 0) {
        --__allocations_before_failure;
    }

    return __real_calloc(count, size);
}

static GLuint __buffer_index(GLuint buffer)
{
    assert(buffer >= 0x90000000u && buffer < 0x90000000u + MAX_NATIVE_OBJECTS);
    return buffer - 0x90000000u;
}

static void __check_buffer(GLuint buffer)
{
    if(buffer) {
        assert(__native_buffers[__buffer_index(buffer)]);
    }
}

void glGenBuffers(GLsizei count, GLuint* buffers)
{
    assert(count == 1);
    if(__fail_native_buffer) {
        *buffers = 0;
        return;
    }

    assert(__next_native_buffer < MAX_NATIVE_OBJECTS);
    __native_buffers[__next_native_buffer] = true;
    *buffers = 0x90000000u + __next_native_buffer++;
    ++__live_buffers;
}

void glBindBuffer(GLenum target, GLuint buffer)
{
    __check_buffer(buffer);
    if(target == GL_ARRAY_BUFFER) {
        __native_array_buffer = buffer;
    } else {
        assert(target == GL_ELEMENT_ARRAY_BUFFER);
        __native_arrays[__native_current_array].element = buffer;
    }
}

void glDeleteBuffers(GLsizei count, const GLuint* buffers)
{
    assert(count == 1);
    GLuint buffer = *buffers;
    __check_buffer(buffer);
    /* Model vitaGL's raw-pointer lifetime: freeing a retained native handle is
     * an error, even when its guest name has already been deleted/reused. */
    assert(__native_array_buffer != buffer);
    for(unsigned index = 0; index < MAX_NATIVE_OBJECTS; ++index) {
        if(!__native_arrays[index].alive) {
            continue;
        }

        assert(__native_arrays[index].element != buffer);
        for(unsigned attrib = 0; attrib < ATTRIB_COUNT; ++attrib) {
            assert(__native_arrays[index].attribs[attrib].buffer != buffer);
        }
    }

    __native_buffers[__buffer_index(buffer)] = false;
    --__live_buffers;
}

GLenum glGetError(void)
{
    GLenum error = __native_error;
    __native_error = GL_NO_ERROR;
    return error;
}

void glGetIntegerv(GLenum name, GLint* values)
{
    assert(values);
    switch(name) {
        case GL_ARRAY_BUFFER_BINDING:
            *values = (GLint)__native_array_buffer;
            break;

        case GL_ELEMENT_ARRAY_BUFFER_BINDING:
            *values = (GLint)__native_arrays[__native_current_array].element;
            break;

        case GL_MAX_VERTEX_ATTRIBS:
            *values = ATTRIB_COUNT;
            break;

        default:
            __native_error = GL_INVALID_ENUM;
    }
}

void glGetFloatv(GLenum name, GLfloat* values)
{
    assert(name == GL_COLOR_CLEAR_VALUE);
    for(unsigned index = 0; index < 4; ++index) {
        values[index] = (GLfloat)index / 4;
    }
}

void glGetBooleanv(GLenum name, GLboolean* values)
{
    assert(name == GL_COLOR_CLEAR_VALUE);
    for(unsigned index = 0; index < 4; ++index) {
        values[index] = index ? GL_TRUE : GL_FALSE;
    }
}

void glGetVertexAttribiv(GLuint index, GLenum name, GLint* values)
{
    assert(index < ATTRIB_COUNT);
    const mock_attrib_t* attrib = &__native_arrays[__native_current_array].attribs[index];
    switch(name) {
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
            *values = (GLint)attrib->buffer;
            break;

        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            *values = attrib->size;
            break;

        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
            *values = attrib->type;
            break;

        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            *values = attrib->normalized;
            break;

        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            *values = attrib->stride;
            break;

        default:
            assert(false);
    }
}

void glGetVertexAttribfv(GLuint index, GLenum name, GLfloat* values)
{
    GLint integer;
    glGetVertexAttribiv(index, name, &integer);
    *values = (GLfloat)integer;
}

void glGetVertexAttribPointerv(GLuint index, GLenum name, void** pointer)
{
    assert(index < ATTRIB_COUNT && name == GL_VERTEX_ATTRIB_ARRAY_POINTER);
    *pointer = (void*)__native_arrays[__native_current_array].attribs[index].pointer;
}

void glVertexAttribPointer(
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
{
    if(__fail_native_attrib) {
        __native_error = GL_INVALID_OPERATION;
        return;
    }

    assert(index < ATTRIB_COUNT);
    __check_buffer(__native_array_buffer);
    __native_arrays[__native_current_array].attribs[index] =
        (mock_attrib_t){__native_array_buffer, size, (GLint)type, normalized, stride, pointer};
}

void glGenVertexArrays(GLsizei count, GLuint* arrays)
{
    assert(count == 1);
    if(__fail_native_array) {
        *arrays = 0;
        return;
    }

    assert(__next_native_array < MAX_NATIVE_OBJECTS);
    *arrays = __next_native_array++;
    __native_arrays[*arrays].alive = true;
    ++__live_arrays;
}

void glBindVertexArray(GLuint array)
{
    assert(array < MAX_NATIVE_OBJECTS && __native_arrays[array].alive);
    __native_current_array = array;
}

void glDeleteVertexArrays(GLsizei count, const GLuint* arrays)
{
    assert(count == 1 && *arrays != __native_current_array);
    assert(__native_arrays[*arrays].alive);
    memset(&__native_arrays[*arrays], 0, sizeof(__native_arrays[*arrays]));
    --__live_arrays;
}

static void __test_buffers(void)
{
    assert(!bridge_gl_is_buffer(0) && !bridge_gl_is_buffer(0xffffffffu));
    GLuint buffers[3];
    bridge_gl_gen_buffers(3, buffers);
    assert(buffers[0] && buffers[0] != buffers[1] && buffers[1] != buffers[2]);
    assert(!bridge_gl_is_buffer(buffers[0]) && !__live_buffers);
    bridge_gl_bind_buffer(GL_ARRAY_BUFFER, buffers[0]);
    bridge_gl_bind_buffer(GL_ELEMENT_ARRAY_BUFFER, buffers[0]);
    assert(bridge_gl_is_buffer(buffers[0]) && __live_buffers == 1);
    GLint binding;
    GLfloat real;
    GLboolean boolean;
    bridge_gl_get_integer_v(GL_ARRAY_BUFFER_BINDING, &binding);
    assert((GLuint)binding == buffers[0]);
    bridge_gl_get_float_v(GL_ELEMENT_ARRAY_BUFFER_BINDING, &real);
    assert(real == (GLfloat)buffers[0]);
    bridge_gl_get_boolean_v(GL_ARRAY_BUFFER_BINDING, &boolean);
    assert(boolean == GL_TRUE);
    bridge_gl_vertex_attrib_pointer(2, 3, GL_FLOAT, GL_FALSE, 20, (void*)8);
    bridge_gl_get_vertex_attrib_iv(2, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &binding);
    assert((GLuint)binding == buffers[0]);
    bridge_gl_get_vertex_attrib_fv(2, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &real);
    assert(real == (GLfloat)buffers[0]);
    bridge_gl_delete_buffers(3, buffers);
    assert(!bridge_gl_is_buffer(buffers[0]) && !__live_buffers);
    assert(!__native_array_buffer && !__native_arrays[0].element);
    assert(!__native_arrays[0].attribs[2].buffer);
    assert(__native_arrays[0].attribs[2].size == 3);
    assert(__native_arrays[0].attribs[2].pointer == (void*)8);
    bridge_gl_delete_buffers(3, buffers);
    /* GLES permits first binding an arbitrary unused name. */
    bridge_gl_bind_buffer(GL_ARRAY_BUFFER, 0xf1234567u);
    assert(bridge_gl_is_buffer(0xf1234567u));
    bridge_gl_get_integer_v(GL_ARRAY_BUFFER_BINDING, &binding);
    assert((GLuint)binding == 0xf1234567u);
    bridge_gl_get_float_v(GL_ARRAY_BUFFER_BINDING, &real);
    assert(real == (GLfloat)0xf1234567u);
    bridge_gl_bind_buffer(GL_ARRAY_BUFFER, 0);
    bridge_gl_get_boolean_v(GL_ARRAY_BUFFER_BINDING, &boolean);
    assert(boolean == GL_FALSE);
    GLfloat color[4];
    GLboolean mask[4];
    bridge_gl_get_float_v(GL_COLOR_CLEAR_VALUE, color);
    bridge_gl_get_boolean_v(GL_COLOR_CLEAR_VALUE, mask);
    assert(color[3] == 0.75f && mask[0] == GL_FALSE && mask[3] == GL_TRUE);
    bridge_gl_shutdown_buffers();
    assert(!__live_buffers);
}

static void __test_retained_objects(void)
{
    GLuint arrays[2];
    bridge_gl_gen_vertex_arrays(2, arrays);
    bridge_gl_bind_vertex_array(arrays[0]);
    bridge_gl_bind_buffer(GL_ARRAY_BUFFER, 77);
    bridge_gl_bind_buffer(GL_ELEMENT_ARRAY_BUFFER, 77);
    bridge_gl_vertex_attrib_pointer(1, 2, GL_SHORT, GL_TRUE, 12, (void*)4);
    GLuint old = __native_array_buffer;
    bridge_gl_bind_vertex_array(arrays[1]);
    GLuint name = 77;
    bridge_gl_delete_buffers(1, &name);
    assert(!bridge_gl_is_buffer(name) && __live_buffers == 1);
    bridge_gl_bind_buffer(GL_ARRAY_BUFFER, name);
    GLuint replacement = __native_array_buffer;
    assert(replacement != old && __live_buffers == 2);
    bridge_gl_bind_vertex_array(arrays[0]);
    GLint binding;
    bridge_gl_get_integer_v(GL_ELEMENT_ARRAY_BUFFER_BINDING, &binding);
    assert(binding == 77);
    bridge_gl_get_vertex_attrib_iv(1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &binding);
    assert(binding == 77);
    __check_buffer(__native_arrays[arrays[0]].element);
    __check_buffer(__native_arrays[arrays[0]].attribs[1].buffer);
    bridge_gl_bind_buffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    assert(__live_buffers == 2);
    __fail_native_attrib = true;
    bridge_gl_vertex_attrib_pointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
    __fail_native_attrib = false;
    assert(bridge_gl_get_error() == GL_INVALID_OPERATION);
    assert(__live_buffers == 2 && __native_arrays[arrays[0]].attribs[1].buffer == old);
    bridge_gl_vertex_attrib_pointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
    assert(__live_buffers == 1 && !__native_buffers[__buffer_index(old)]);
    /* A current VAO deletion binds the default VAO and releases retained refs. */
    bridge_gl_delete_vertex_arrays(2, arrays);
    assert(!__live_arrays && __native_current_array == 0);
    bridge_gl_shutdown_buffers();
    assert(!__live_buffers);
    /* A deleted buffer referenced only by an unbound default VAO must survive
     * until teardown clears that final reference. */
    bridge_gl_bind_buffer(GL_ARRAY_BUFFER, 42);
    bridge_gl_vertex_attrib_pointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    bridge_gl_gen_vertex_arrays(1, arrays);
    bridge_gl_bind_vertex_array(arrays[0]);
    name = 42;
    bridge_gl_delete_buffers(1, &name);
    assert(__live_buffers == 1);
    bridge_gl_shutdown_buffers();
    assert(!__live_buffers && !__live_arrays);
}

static void __test_failures(void)
{
    GLuint names[3] = {99, 99, 99};

    bridge_gl_gen_buffers(-1, names);
    assert(names[0] == 99 && bridge_gl_get_error() == GL_INVALID_VALUE);
    bridge_gl_delete_buffers(-1, names);
    assert(bridge_gl_get_error() == GL_INVALID_VALUE);
    bridge_gl_bind_buffer(0xdead, 21);
    assert(!bridge_gl_is_buffer(21) && bridge_gl_get_error() == GL_INVALID_ENUM);
    __native_error = GL_INVALID_OPERATION;
    bridge_gl_gen_buffers(1, NULL);
    assert(bridge_gl_get_error() == GL_INVALID_VALUE);
    assert(bridge_gl_get_error() == GL_INVALID_OPERATION);
    assert(bridge_gl_get_error() == GL_NO_ERROR);
    __allocations_before_failure = 1;
    bridge_gl_gen_buffers(3, names);
    __allocations_before_failure = -1;
    assert(!names[0] && !names[1] && !names[2]);
    assert(bridge_gl_get_error() == GL_OUT_OF_MEMORY);
    __fail_native_buffer = true;
    bridge_gl_bind_buffer(GL_ARRAY_BUFFER, 22);
    __fail_native_buffer = false;
    assert(!bridge_gl_is_buffer(22) && !__live_buffers);
    assert(bridge_gl_get_error() == GL_OUT_OF_MEMORY);
    bridge_gl_bind_buffer(GL_ARRAY_BUFFER, 22);
    assert(bridge_gl_is_buffer(22));
    bridge_gl_vertex_attrib_pointer(ATTRIB_COUNT, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    assert(bridge_gl_get_error() == GL_INVALID_VALUE);
    bridge_gl_vertex_attrib_pointer(0, 4, 0xdead, GL_FALSE, 0, NULL);
    assert(bridge_gl_get_error() == GL_INVALID_ENUM);
    __allocations_before_failure = 0;
    bridge_gl_vertex_attrib_pointer(7, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    __allocations_before_failure = -1;
    assert(bridge_gl_get_error() == GL_OUT_OF_MEMORY);
    bridge_gl_bind_vertex_array(0xffffffffu);
    assert(bridge_gl_get_error() == GL_INVALID_OPERATION);
    __allocations_before_failure = 1;
    bridge_gl_gen_vertex_arrays(3, names);
    __allocations_before_failure = -1;
    assert(!__live_arrays && !names[0] && !names[1] && !names[2]);
    assert(bridge_gl_get_error() == GL_OUT_OF_MEMORY);
    __fail_native_array = true;
    bridge_gl_gen_vertex_arrays(1, names);
    __fail_native_array = false;
    assert(!names[0] && bridge_gl_get_error() == GL_OUT_OF_MEMORY);
    bridge_gl_shutdown_buffers();
    assert(!__live_buffers && !__live_arrays && bridge_gl_get_error() == GL_NO_ERROR);
}

static GLint __native_rows;
static unsigned __uploads;
static unsigned char __uploaded[128];
static const void* __upload_source;
static GLsizei __upload_width, __upload_height;

void glPixelStorei(GLenum name, GLint value)
{
    if(name == GL_UNPACK_ROW_LENGTH) {
        __native_rows = value;
    } else {
        __native_error = GL_INVALID_ENUM;
    }
}

static void __upload(GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)
{
    ++__uploads;
    __upload_source = pixels;
    __upload_width = width;
    __upload_height = height;
    if(!pixels || width <= 0 || height <= 0) {
        return;
    }

    size_t bytes = type == GL_UNSIGNED_SHORT_5_6_5 ? 2 : format == GL_RGB ? 3 : format == GL_RGBA ? 4 : 1;
    assert((size_t)width * height * bytes <= sizeof(__uploaded));
    size_t stride = (size_t)(__native_rows ? __native_rows : width) * bytes;
    for(GLsizei row = 0; row < height; ++row) {
        memcpy(__uploaded + (size_t)row * width * bytes,
            (const unsigned char*)pixels + (size_t)row * stride,
            (size_t)width * bytes);
    }
}

void glTexImage2D(GLenum target, GLint level, GLint internal_format, GLsizei width, GLsizei height, GLint border,
    GLenum format, GLenum type, const void* pixels)
{
    assert(target == GL_TEXTURE_2D && level == 0 && border == 0 && internal_format == (GLint)format);
    __upload(width, height, format, type, pixels);
}

void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
    GLenum type, const void* pixels)
{
    assert(target == GL_TEXTURE_2D && level == 0 && x == 2 && y == 3);
    __upload(width, height, format, type, pixels);
}

static void __test_unpack(void)
{
    GLint value = 0;
    bridge_gl_get_integer_v(GL_UNPACK_ALIGNMENT, &value);
    assert(value == 4 && bridge_gl_get_error() == GL_NO_ERROR);
    const unsigned char padded[] = {1, 2, 3, 99, 4, 5, 6, 98};

    const unsigned char packed[] = {1, 2, 3, 4, 5, 6};

    bridge_gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, padded);
    assert(!memcmp(__uploaded, packed, sizeof(packed)));
    assert(__upload_source != padded && __upload_width == 1 && __upload_height == 2);
    bridge_gl_pixel_store_i(GL_UNPACK_ALIGNMENT, 1);
    bridge_gl_tex_sub_image_2d(GL_TEXTURE_2D, 0, 2, 3, 1, 2, GL_RGB, GL_UNSIGNED_BYTE, packed);
    assert(__upload_source == packed && !memcmp(__uploaded, packed, sizeof(packed)));
    bridge_gl_pixel_store_i(GL_UNPACK_ALIGNMENT, 8);
    bridge_gl_pixel_store_i(GL_UNPACK_ROW_LENGTH, 2);
    const unsigned char rows[] = {1, 2, 3, 91, 92, 93, 94, 95, 4, 5, 6};

    bridge_gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rows);
    assert(!memcmp(__uploaded, packed, sizeof(packed)) && __native_rows == 2);
    bridge_gl_get_integer_v(GL_UNPACK_ROW_LENGTH, &value);
    assert(value == 2);
    GLfloat real = 0;
    GLboolean boolean = 0;
    bridge_gl_get_float_v(GL_UNPACK_ALIGNMENT, &real);
    bridge_gl_get_boolean_v(GL_UNPACK_ROW_LENGTH, &boolean);
    assert(real == 8.0f && boolean == GL_TRUE);
    bridge_gl_pixel_store_i(GL_UNPACK_ALIGNMENT, 3);
    bridge_gl_pixel_store_i(GL_UNPACK_ROW_LENGTH, -1);
    assert(bridge_gl_get_error() == GL_INVALID_VALUE && bridge_gl_get_error() == GL_NO_ERROR);
    bridge_gl_get_integer_v(GL_UNPACK_ALIGNMENT, &value);
    assert(value == 8 && __native_rows == 2);
    unsigned previous = __uploads;
    __allocations_before_failure = 0;
    bridge_gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rows);
    __allocations_before_failure = -1;
    assert(__uploads == previous && __native_rows == 2 && bridge_gl_get_error() == GL_OUT_OF_MEMORY);
    bridge_gl_reset_pixel_store();
    const unsigned char rgb565[] = {1, 2, 99, 98, 3, 4};

    const unsigned char expected565[] = {1, 2, 3, 4};

    bridge_gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, rgb565);
    assert(!memcmp(__uploaded, expected565, sizeof(expected565)));
    bridge_gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    assert(!__upload_source && __upload_width == 2 && __upload_height == 2);
    bridge_gl_pixel_store_i(0xdead, 1);
    assert(bridge_gl_get_error() == GL_INVALID_ENUM);
    assert(bridge_gl_get_error() == GL_NO_ERROR);
    bridge_gl_reset_pixel_store();
}

/* Dynamic lookup tests are defined in a generated fixture from symbol.def. */
void test_gl_proc_addresses(void);

int main(void)
{
    __test_buffers();
    __test_retained_objects();
    __test_failures();
    __test_unpack();
    test_gl_proc_addresses();
    assert(!__live_buffers && !__live_arrays);
    puts("OpenGL namespace, VAO retention, failure and canonical proc tests passed");
    return 0;
}
