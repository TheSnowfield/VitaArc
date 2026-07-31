#include <stdlib.h>
#include <vitaGL.h>
#include <stdio.h>
#include <string.h>
#include <psp2/kernel/processmgr.h>

#include <config.h>
#include <logcat/logcat.h>
#include <utils/fs.h>
#include "gl.h"
#include "shader_cache.h"

#define MAX_TRACKED_SHADERS 256

typedef struct gl_shader_t {
    GLuint handle;
    GLenum type;
    uint64_t hash;
    bool has_source;
} gl_shader_t;

static gl_shader_t __tracked_shaders[MAX_TRACKED_SHADERS];

static gl_shader_t* __track_shader(GLuint handle)
{
    if(!handle) {
        return NULL;
    }

    gl_shader_t* empty = NULL;
    for(size_t index = 0; index < MAX_TRACKED_SHADERS; ++index) {
        if(__tracked_shaders[index].handle == handle) {
            return &__tracked_shaders[index];
        }

        if(!empty && __tracked_shaders[index].handle == 0) {
            empty = &__tracked_shaders[index];
        }
    }

    if(empty) {
        empty->handle = handle;
    }

    return empty;
}

GLuint bridge_gl_create_shader(GLenum type)
{
    GLenum native_type = type;
    if(bridge_shader_is_offline()) {
        switch(type) {
            case GL_VERTEX_SHADER:
                native_type = GL_CG_VERTEX_SHADER_EXT;
                break;

            case GL_FRAGMENT_SHADER:
                native_type = GL_CG_FRAGMENT_SHADER_EXT;
                break;
        }
    }

    GLuint handle = glCreateShader(native_type);
    gl_shader_t* tracked = __track_shader(handle);
    if(tracked) {
        *tracked = (gl_shader_t){.handle = handle, .type = type};
    }

    return handle;
}

void bridge_gl_delete_shader(GLuint shader)
{
    for(size_t index = 0; index < MAX_TRACKED_SHADERS; ++index) {
        if(__tracked_shaders[index].handle == shader) {
            __tracked_shaders[index] = (gl_shader_t){0};

            break;
        }
    }

    glDeleteShader(shader);
}

void bridge_gl_link_program(GLuint program)
{
    glLinkProgram(program);
    if(bridge_shader_is_offline()) {
        GLint status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if(status != GL_TRUE) {
            log_f(TAG, "Offline shader program %u link failed", program);
            sceKernelExitProcess(1);
            return;
        }

        log_i(TAG, "Linked offline shader program %u", program);
    }
}

void bridge_gl_bind_renderbuffer(GLenum target, GLuint renderbuffer)
{
    glBindRenderbuffer(target, renderbuffer);
}

void bridge_gl_delete_renderbuffers(GLsizei n, const GLuint* renderbuffers)
{
    glDeleteRenderbuffers(n, renderbuffers);
}

void bridge_gl_gen_renderbuffers(GLsizei n, GLuint* renderbuffers)
{
    glGenRenderbuffers(n, renderbuffers);
}

void bridge_gl_framebuffer_renderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffer_target, GLuint renderbuffer)
{
    glFramebufferRenderbuffer(target, attachment, renderbuffer_target, renderbuffer);
}

void bridge_gl_renderbuffer_storage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    glRenderbufferStorage(target, internalformat, width, height);
}

GLboolean bridge_gl_is_renderbuffer(GLuint renderbuffer)
{
    return glIsRenderbuffer(renderbuffer);
}

void* bridge_gl_map_buffer(GLenum target, GLenum access)
{
    return glMapBuffer(target, access);
}

GLboolean bridge_gl_unmap_buffer(GLenum target)
{
    return glUnmapBuffer(target);
}

void _gl_shader_source(GLuint handle, GLsizei count, const GLchar* const* string, const GLint* length)
{
    if(count <= 0 || !string) {
        glShaderSource(handle, count, string, length);
        return;
    }

    size_t size = 0;
    for(GLsizei index = 0; index < count; ++index) {
        if(!string[index]) {
            continue;
        }

        size_t part_size = length && length[index] >= 0 ? (size_t)length[index] : strlen(string[index]);
        if(part_size >= SIZE_MAX - size) {
            log_e(TAG, "Shader source length overflow for handle %u", handle);
            return;
        }

        size += part_size;
    }

    char* original = malloc(size + 1);
    if(!original) {
        return;
    }

    char* output = original;
    for(GLsizei index = 0; index < count; ++index) {
        if(string[index]) {
            size_t part_size = length && length[index] >= 0 ? (size_t)length[index] : strlen(string[index]);
            memcpy(output, string[index], part_size);
            output += part_size;
        }
    }

    original[size] = '\0';

    GLint type = 0;
    glGetShaderiv(handle, GL_SHADER_TYPE, &type);
    uint64_t hash = bridge_shader_hash(original, size);
    gl_shader_t* tracked = __track_shader(handle);
    if(tracked) {
        if(!bridge_shader_is_offline()) {
            tracked->type = (GLenum)type;
        }

        tracked->hash = hash;
        tracked->has_source = true;
    }

    if(bridge_shader_is_offline()) {
        bridge_shader_dump_missing(hash, tracked ? tracked->type : (GLenum)type, original, size);
        const GLchar* source = original;
        GLint source_length = (GLint)size;
        glShaderSource(handle, 1, &source, &source_length);
        free(original);
        return;
    }

    size_t replacement_size = 0;
    char* replacement = bridge_shader_load_replacement(hash, &replacement_size);
    if(replacement) {
        const GLchar* replacement_source = replacement;
        GLint replacement_length = (GLint)replacement_size;
        glShaderSource(handle, 1, &replacement_source, &replacement_length);
    } else {
        bridge_shader_dump_missing(hash, (GLenum)type, original, size);
        log_w(TAG,
            "No replacement for %s shader %016llx; forwarding original",
            type == GL_VERTEX_SHADER ? "vertex" : "fragment",
            (unsigned long long)hash);
        const GLchar* original_source = original;
        GLint original_length = (GLint)size;
        glShaderSource(handle, 1, &original_source, &original_length);
    }

    free(replacement);
    free(original);
}

void _gl_compile_shader(GLuint handle)
{
    gl_shader_t* tracked = __track_shader(handle);
    if(bridge_shader_is_offline()) {
        size_t size = 0;
        void* binary =
            tracked && tracked->has_source ? bridge_shader_load_binary(tracked->hash, tracked->type, &size) : NULL;
        if(!binary) {
            log_f(TAG,
                "Offline shader unavailable: %016llx (type %x); source dumped to ux0:vitaarc/shaders/missing",
                (unsigned long long)(tracked ? tracked->hash : 0),
                tracked ? tracked->type : 0);
            sceKernelExitProcess(1);
            return;
        }

        glShaderBinary(1, &handle, 0, binary, (GLsizei)size);
        free(binary);
    } else {
        glCompileShader(handle);
    }

    GLint status = GL_FALSE;
    GLint log_length = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    uint64_t hash = tracked ? tracked->hash : 0;
    if(status == GL_TRUE) {
        log_i(TAG, "Compiled shader %016llx", (unsigned long long)hash);
        return;
    }

    if(log_length < 1) {
        log_length = 1;
    }

    char* info_log = calloc((size_t)log_length + 1, 1);
    if(info_log) {
        glGetShaderInfoLog(handle, log_length, NULL, info_log);
        log_e(TAG, "Shader %016llx compile failed: %s", (unsigned long long)hash, info_log);
        free(info_log);
    } else {
        log_e(TAG, "Shader %016llx compile failed", (unsigned long long)hash);
    }

    if(bridge_shader_is_offline()) {
        sceKernelExitProcess(1);
    }
}

void _gl_get_shader_source(GLuint shader, GLsizei buffer_size, GLsizei* length, GLchar* source)
{
    glGetShaderSource(shader, buffer_size, length, source);
}

void _gl_bind_attrib_location(GLuint program, GLuint index, const GLchar* name)
{
    log_i(TAG, "_gl_bind_attrib_location(%d, %d, %s)", program, index, name);
    glBindAttribLocation(program, index, name);
}
