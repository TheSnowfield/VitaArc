#ifndef __BRIDGES_OPENGL_SHADER_BINARY_H
#define __BRIDGES_OPENGL_SHADER_BINARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Installed vitaGL ABI: matrix count, parameter indices, then raw GXP.
 * Check the envelope and reflection bounds before vitaGL deserializes it.
 * This is not a GPU instruction verifier; binaries are local build assets. */
bool bridge_shader_binary_validate(const void* binary, size_t size, uint32_t shader_type);

#endif
