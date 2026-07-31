#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "buffers.h"
#include "pixel_store.h"

/* The game uses a single GL context on its render owner thread. Guest names
 * must never be passed as vitaGL's pointer-valued native buffer handles. */
typedef struct bridge_buffer {
    GLuint name;
    GLuint native;
    bool created;
    bool deleted;
    struct bridge_buffer* next;
} bridge_buffer_t;

typedef struct bridge_attrib {
    GLuint index;
    bridge_buffer_t* buffer;
    struct bridge_attrib* next;
} bridge_attrib_t;

typedef struct bridge_vertex_array {
    GLuint native;
    bridge_buffer_t* element;
    bridge_attrib_t* attribs;
    struct bridge_vertex_array* next;
} bridge_vertex_array_t;

static bridge_buffer_t* __buffers;
static bridge_buffer_t* __array_buffer;
static bridge_vertex_array_t __default_array;
static bridge_vertex_array_t* __arrays = &__default_array;
static bridge_vertex_array_t* __current_array = &__default_array;
static GLuint __next_name = 1;
static GLenum __error;

static void __set_error(GLenum error)
{
    if(__error == GL_NO_ERROR) {
        __error = error;
    }
}

static bridge_buffer_t* __find_buffer(GLuint name)
{
    for(bridge_buffer_t* buffer = __buffers; buffer; buffer = buffer->next) {
        if(buffer->name == name && !buffer->deleted) {
            return buffer;
        }
    }

    return NULL;
}

static bool __is_referenced(bridge_buffer_t* buffer)
{
    if(__array_buffer == buffer) {
        return true;
    }

    for(bridge_vertex_array_t* array = __arrays; array; array = array->next) {
        if(array->element == buffer) {
            return true;
        }

        for(bridge_attrib_t* attrib = array->attribs; attrib; attrib = attrib->next) {
            if(attrib->buffer == buffer) {
                return true;
            }
        }
    }

    return false;
}

static void __collect_deleted(void)
{
    bridge_buffer_t** link = &__buffers;
    while(*link) {
        bridge_buffer_t* buffer = *link;
        if(!buffer->deleted || __is_referenced(buffer)) {
            link = &buffer->next;
            continue;
        }

        *link = buffer->next;
        if(buffer->native) {
            glDeleteBuffers(1, &buffer->native);
        }

        free(buffer);
    }
}

static bridge_buffer_t* __find_native(GLuint native)
{
    for(bridge_buffer_t* buffer = __buffers; buffer; buffer = buffer->next) {
        if(buffer->native == native && native) {
            return buffer;
        }
    }

    return NULL;
}

static GLuint __guest_name(GLuint native)
{
    bridge_buffer_t* buffer = __find_native(native);
    return buffer ? buffer->name : 0;
}

static bool __is_binding_query(GLenum name)
{
    return name == GL_ARRAY_BUFFER_BINDING || name == GL_ELEMENT_ARRAY_BUFFER_BINDING;
}

static GLuint __allocate_name(void)
{
    GLuint first = __next_name;
    do {
        GLuint candidate = __next_name++;
        if(__next_name == 0) {
            __next_name = 1;
        }

        if(!__find_buffer(candidate)) {
            return candidate;
        }
    } while(__next_name != first);

    return 0;
}

static bridge_buffer_t* __reserve_buffer(GLuint name)
{
    bridge_buffer_t* buffer = calloc(1, sizeof(*buffer));
    if(!buffer) {
        __set_error(GL_OUT_OF_MEMORY);
        return NULL;
    }

    buffer->name = name;
    buffer->next = __buffers;
    __buffers = buffer;
    return buffer;
}

void bridge_gl_gen_buffers(GLsizei count, GLuint* buffers)
{
    if(count < 0 || (count && !buffers)) {
        __set_error(GL_INVALID_VALUE);
        return;
    }

    bridge_buffer_t* previous = __buffers;
    for(GLsizei index = 0; index < count; ++index) {
        GLuint name = __allocate_name();
        if(!name || !__reserve_buffer(name)) {
            __set_error(GL_OUT_OF_MEMORY);
            while(__buffers != previous) {
                bridge_buffer_t* buffer = __buffers;
                __buffers = buffer->next;
                free(buffer);
            }

            for(GLsizei clear = 0; clear < count; ++clear) {
                buffers[clear] = 0;
            }

            return;
        }

        buffers[index] = name;
    }
}

void bridge_gl_bind_buffer(GLenum target, GLuint name)
{
    if(target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
        __set_error(GL_INVALID_ENUM);
        return;
    }

    if(!name) {
        glBindBuffer(target, 0);
        if(target == GL_ARRAY_BUFFER) {
            __array_buffer = NULL;
        } else {
            __current_array->element = NULL;
        }

        __collect_deleted();
        return;
    }

    bridge_buffer_t* buffer = __find_buffer(name);
    bool reserved = buffer != NULL;
    if(!buffer) {
        buffer = __reserve_buffer(name);
    }

    if(!buffer) {
        return;
    }

    if(!buffer->native) {
        glGenBuffers(1, &buffer->native);
        if(!buffer->native) {
            __set_error(GL_OUT_OF_MEMORY);
            if(!reserved) {
                __buffers = buffer->next;
                free(buffer);
            }

            return;
        }
    }

    glBindBuffer(target, buffer->native);
    buffer->created = true;
    if(target == GL_ARRAY_BUFFER) {
        __array_buffer = buffer;
    } else {
        __current_array->element = buffer;
    }

    __collect_deleted();
}

GLboolean bridge_gl_is_buffer(GLuint name)
{
    bridge_buffer_t* buffer = __find_buffer(name);
    return buffer && buffer->created ? GL_TRUE : GL_FALSE;
}

static void __detach_attrib(bridge_attrib_t* attrib)
{
    GLint size = 0, type = 0, normalized = 0, stride = 0;
    void* pointer = NULL;
    glGetVertexAttribiv(attrib->index, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    glGetVertexAttribiv(attrib->index, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
    glGetVertexAttribiv(attrib->index, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
    glGetVertexAttribiv(attrib->index, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    glGetVertexAttribPointerv(attrib->index, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(attrib->index, size, (GLenum)type, (GLboolean)normalized, stride, pointer);
    glBindBuffer(GL_ARRAY_BUFFER, __array_buffer ? __array_buffer->native : 0);
    attrib->buffer = NULL;
}

void bridge_gl_delete_buffers(GLsizei count, const GLuint* buffers)
{
    if(count < 0 || (count && !buffers)) {
        __set_error(GL_INVALID_VALUE);
        return;
    }

    for(GLsizei index = 0; index < count; ++index) {
        bridge_buffer_t* buffer = __find_buffer(buffers[index]);
        if(!buffer) {
            continue;
        }

        buffer->deleted = true;
        if(__array_buffer == buffer) {
            __array_buffer = NULL;
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        if(__current_array->element == buffer) {
            __current_array->element = NULL;
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

        /* Deletion detaches the currently bound VAO. Other VAOs retain their
         * object references until modified or deleted, even if the name is reused. */
        for(bridge_attrib_t* attrib = __current_array->attribs; attrib; attrib = attrib->next) {
            if(attrib->buffer == buffer) {
                __detach_attrib(attrib);
            }
        }

        __collect_deleted();
    }
}

void bridge_gl_vertex_attrib_pointer(
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
{
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maximum);
    if(index >= (GLuint)maximum || size < 1 || size > 4 || stride < 0) {
        __set_error(GL_INVALID_VALUE);
        return;
    }

    if(type != GL_BYTE && type != GL_UNSIGNED_BYTE && type != GL_SHORT && type != GL_UNSIGNED_SHORT &&
        type != GL_FLOAT && type != GL_FIXED) {
        __set_error(GL_INVALID_ENUM);
        return;
    }

    bridge_attrib_t* attrib = __current_array->attribs;
    while(attrib && attrib->index != index) {
        attrib = attrib->next;
    }

    if(!attrib) {
        attrib = calloc(1, sizeof(*attrib));
        if(!attrib) {
            __set_error(GL_OUT_OF_MEMORY);
            return;
        }

        attrib->index = index;
        attrib->next = __current_array->attribs;
        __current_array->attribs = attrib;
    }

    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
    /* Query the committed native binding: an implementation-specific rejection
     * must not discard the old buffer reference or consume the native error. */
    GLint binding = 0;
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &binding);
    attrib->buffer = __find_native((GLuint)binding);
    __collect_deleted();
}

void bridge_gl_gen_vertex_arrays(GLsizei count, GLuint* arrays)
{
    if(count < 0 || (count && !arrays)) {
        __set_error(GL_INVALID_VALUE);
        return;
    }

    for(GLsizei index = 0; index < count; ++index) {
        bridge_vertex_array_t* array = calloc(1, sizeof(*array));
        if(array) {
            glGenVertexArrays(1, &array->native);
        }

        if(!array || !array->native) {
            free(array);
            __set_error(GL_OUT_OF_MEMORY);
            bridge_gl_delete_vertex_arrays(index, arrays);
            for(GLsizei clear = 0; clear < count; ++clear) {
                arrays[clear] = 0;
            }

            return;
        }

        array->next = __arrays;
        __arrays = array;
        arrays[index] = array->native;
    }
}

void bridge_gl_bind_vertex_array(GLuint name)
{
    for(bridge_vertex_array_t* array = __arrays; array; array = array->next) {
        if(array->native == name) {
            glBindVertexArray(name);
            __current_array = array;
            return;
        }
    }

    __set_error(GL_INVALID_OPERATION);
}

static void __free_attribs(bridge_vertex_array_t* array)
{
    while(array->attribs) {
        bridge_attrib_t* attrib = array->attribs;
        array->attribs = attrib->next;
        free(attrib);
    }
}

void bridge_gl_delete_vertex_arrays(GLsizei count, const GLuint* arrays)
{
    if(count < 0 || (count && !arrays)) {
        __set_error(GL_INVALID_VALUE);
        return;
    }

    for(GLsizei index = 0; index < count; ++index) {
        if(!arrays[index]) {
            continue;
        }

        bridge_vertex_array_t** link = &__arrays;
        while(*link && (*link)->native != arrays[index]) {
            link = &(*link)->next;
        }

        if(!*link) {
            continue;
        }

        bridge_vertex_array_t* array = *link;
        if(__current_array == array) {
            bridge_gl_bind_vertex_array(0);
        }

        *link = array->next;
        glDeleteVertexArrays(1, &array->native);
        __free_attribs(array);
        free(array);
        __collect_deleted();
    }
}

GLenum bridge_gl_get_error(void)
{
    if(__error != GL_NO_ERROR) {
        GLenum error = __error;
        __error = GL_NO_ERROR;
        return error;
    }

    return glGetError();
}

void bridge_gl_set_error(GLenum error)
{
    __set_error(error);
}

void bridge_gl_get_integer_v(GLenum name, GLint* values)
{
    if(bridge_gl_query_pixel_store(name, values)) {
        return;
    }

    glGetIntegerv(name, values);
    if(values && __is_binding_query(name)) {
        *values = (GLint)__guest_name((GLuint)*values);
    }
}

void bridge_gl_get_float_v(GLenum name, GLfloat* values)
{
    GLint pixel;
    if(values && bridge_gl_query_pixel_store(name, &pixel)) {
        *values = (GLfloat)pixel;
        return;
    }

    if(values && __is_binding_query(name)) {
        GLint integer = 0;
        bridge_gl_get_integer_v(name, &integer);
        *values = (GLfloat)(GLuint)integer;
    } else {
        glGetFloatv(name, values);
    }
}

void bridge_gl_get_boolean_v(GLenum name, GLboolean* values)
{
    GLint pixel;
    if(values && bridge_gl_query_pixel_store(name, &pixel)) {
        *values = pixel ? GL_TRUE : GL_FALSE;
        return;
    }

    if(values && __is_binding_query(name)) {
        GLint integer = 0;
        bridge_gl_get_integer_v(name, &integer);
        *values = integer ? GL_TRUE : GL_FALSE;
    } else {
        glGetBooleanv(name, values);
    }
}

void bridge_gl_get_vertex_attrib_iv(GLuint index, GLenum name, GLint* values)
{
    glGetVertexAttribiv(index, name, values);
    if(values && name == GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING) {
        *values = (GLint)__guest_name((GLuint)*values);
    }
}

void bridge_gl_get_vertex_attrib_fv(GLuint index, GLenum name, GLfloat* values)
{
    if(values && name == GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING) {
        GLint integer = 0;
        bridge_gl_get_vertex_attrib_iv(index, name, &integer);
        *values = (GLfloat)(GLuint)integer;
    } else {
        glGetVertexAttribfv(index, name, values);
    }
}

void bridge_gl_shutdown_buffers(void)
{
    bridge_gl_bind_vertex_array(0);
    while(__arrays != &__default_array) {
        GLuint array = __arrays->native;
        bridge_gl_delete_vertex_arrays(1, &array);
    }

    for(bridge_attrib_t* attrib = __default_array.attribs; attrib; attrib = attrib->next) {
        if(attrib->buffer) {
            __detach_attrib(attrib);
        }
    }

    __free_attribs(&__default_array);
    __default_array.element = NULL;
    __array_buffer = NULL;
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    while(__buffers) {
        bridge_buffer_t* buffer = __buffers;
        __buffers = buffer->next;
        if(buffer->native) {
            glDeleteBuffers(1, &buffer->native);
        }

        free(buffer);
    }

    __next_name = 1;
    __error = GL_NO_ERROR;
    bridge_gl_reset_pixel_store();
}
