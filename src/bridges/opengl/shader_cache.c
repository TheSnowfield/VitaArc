#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include <config.h>
#include <logcat/logcat.h>
#include <utils/fs.h>

#include "shader_cache.h"
#include "shader_binary.h"

#define SHADER_APP_PATH PATH_TO_APP "/shaders/"
#define SHADER_USER_PATH PATH_TO_USER "shaders/"
#define SHADER_MISSING_PATH SHADER_USER_PATH "missing/"

static bool __offline;

bool bridge_shader_is_offline(void)
{
    return __offline;
}

uint64_t bridge_shader_hash(const char* source, size_t length)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for(size_t index = 0; index < length; ++index) {
        hash ^= (uint8_t)source[index];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

static char* __read_shader(const char* path, size_t* length)
{
    if(!util_file_exists(path)) {
        return NULL;
    }

    uint32_t file_size = util_get_file_size(path);
    if(file_size == 0 || file_size > 1024 * 1024) {
        return NULL;
    }

    char* source = malloc((size_t)file_size + 1);
    if(!source || !read_file_all(path, source, file_size)) {
        free(source);
        return NULL;
    }

    source[file_size] = '\0';
    if(length) {
        *length = file_size;
    }

    return source;
}

bool bridge_shader_enable_offline(void)
{
    const char* paths[] = {SHADER_USER_PATH "offline.enabled", SHADER_APP_PATH "offline.enabled"};

    __offline = false;
    for(size_t index = 0; index < sizeof(paths) / sizeof(paths[0]); ++index) {
        size_t length = 0;
        char* marker = __read_shader(paths[index], &length);
        const char expected[] = "VitaArc offline shaders 1\n";
        bool valid = marker && length == sizeof(expected) - 1 && memcmp(marker, expected, length) == 0;
        free(marker);
        if(valid) {
            __offline = true;
            return true;
        }
    }

    return false;
}

void* bridge_shader_load_binary(uint64_t hash, GLenum type, size_t* length)
{
    const char* roots[] = {SHADER_USER_PATH, SHADER_APP_PATH};

    for(size_t index = 0; index < sizeof(roots) / sizeof(roots[0]); ++index) {
        char path[256];
        snprintf(path, sizeof(path), "%s%016llx.vgxp", roots[index], (unsigned long long)hash);
        size_t size = 0;
        char* binary = __read_shader(path, &size);
        if(!binary) {
            continue;
        }

        if(!bridge_shader_binary_validate(binary, size, type)) {
            log_e(TAG, "Invalid offline shader binary %s", path);
            free(binary);
            return NULL;
        }

        *length = size;
        return binary;
    }

    return NULL;
}

char* bridge_shader_load_replacement(uint64_t hash, size_t* length)
{
    char path[256];
    snprintf(path, sizeof(path), SHADER_USER_PATH "%016llx.cg", (unsigned long long)hash);
    char* source = __read_shader(path, length);
    if(source) {
        log_i(TAG, "Using user shader replacement %016llx", (unsigned long long)hash);
        return source;
    }

    snprintf(path, sizeof(path), SHADER_APP_PATH "%016llx.cg", (unsigned long long)hash);
    source = __read_shader(path, length);
    if(source) {
        log_i(TAG, "Using packaged shader replacement %016llx", (unsigned long long)hash);
    }

    return source;
}

void bridge_shader_dump_missing(uint64_t hash, GLenum type, const char* source, size_t length)
{
    sceIoMkdir(PATH_TO_USER, 0777);
    sceIoMkdir(SHADER_USER_PATH, 0777);
    sceIoMkdir(SHADER_MISSING_PATH, 0777);

    const char* suffix = type == GL_VERTEX_SHADER ? "vert" : "frag";
    char path[256];
    snprintf(path, sizeof(path), SHADER_MISSING_PATH "%016llx.%s.glsl", (unsigned long long)hash, suffix);
    if(util_file_exists(path)) {
        return;
    }

    SceUID file = sceIoOpen(path, SCE_O_CREAT | SCE_O_WRONLY | SCE_O_TRUNC, 0777);
    if(file < 0) {
        log_w(TAG, "Could not dump missing shader %016llx", (unsigned long long)hash);
        return;
    }

    sceIoWrite(file, source, length);
    sceIoClose(file);
}
