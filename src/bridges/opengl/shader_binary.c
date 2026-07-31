#include <string.h>

#include "shader_binary.h"

static uint32_t read_u32(const unsigned char* bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

bool bridge_shader_binary_validate(const void* binary, size_t size, uint32_t shader_type)
{
    if(!binary || size < 4 + 156 || size > 1024 * 1024 || (shader_type != 0x8b31 && shader_type != 0x8b30)) {
        return false;
    }

    const unsigned char* bytes = binary;
    uint32_t matrices = read_u32(bytes);
    if(matrices > 64 || (size_t)matrices * 4 > size - 4 - 156) {
        return false;
    }

    size_t header = (1 + (size_t)matrices) * 4;
    const unsigned char* gxp = bytes + header;
    size_t available = size - header;
    uint32_t length = read_u32(gxp + 8);
    if(read_u32(gxp) != UINT32_C(0x00505847) || length < 156 || length > available || available - length > 3 ||
        (read_u32(gxp + 20) & 1) != (shader_type == 0x8b30)) {
        return false;
    }

    uint32_t count = read_u32(gxp + 0x24);
    uint32_t relative = read_u32(gxp + 0x28);
    if(relative > length - 0x28) {
        return false;
    }

    size_t table = 0x28 + (size_t)relative;
    if(count > (length - table) / 16) {
        return false;
    }

    for(uint32_t index = 0; index < count; ++index) {
        size_t entry = table + (size_t)index * 16;
        uint32_t name = read_u32(gxp + entry);
        if(name >= length - entry || !memchr(gxp + entry + name, 0, length - entry - name)) {
            return false;
        }
    }

    for(uint32_t matrix = 0; matrix < matrices; ++matrix) {
        uint32_t index = read_u32(bytes + (1 + matrix) * 4);
        if(index >= count || (gxp[table + (size_t)index * 16 + 4] & 0xf) != 1) {
            return false;
        }

        for(uint32_t previous = 0; previous < matrix; ++previous) {
            if(index == read_u32(bytes + (1 + previous) * 4)) {
                return false;
            }
        }
    }

    return true;
}
