#ifndef __BRIDGES_OPENGL_IMPL_SHADER_CACHE_H
#define __BRIDGES_OPENGL_IMPL_SHADER_CACHE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <vitaGL.h>

uint64_t bridge_shader_hash(const char* source, size_t length);
bool bridge_shader_enable_offline(void);
bool bridge_shader_is_offline(void);
void* bridge_shader_load_binary(uint64_t hash, GLenum type, size_t* length);
char* bridge_shader_load_replacement(uint64_t hash, size_t* length);
void bridge_shader_dump_missing(uint64_t hash, GLenum type, const char* source, size_t length);

#endif /* __BRIDGES_OPENGL_IMPL_SHADER_CACHE_H */
