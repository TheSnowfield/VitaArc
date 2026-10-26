#include <stdint.h>
#include <string.h>

#include "texture_size.h"

bool vita_texture_small_size(unsigned* width, unsigned* height)
{
    if(!width || !height || !*width || !*height || *width > 16384 || *height > 16384) {
        return false;
    }

    unsigned small = *width < *height ? *width : *height;
    unsigned large = *width > *height ? *width : *height;
    unsigned limit = small < 64 ? 4096 : 960;
    if(small >= 64 && large >= 256) {
        *width = (*width + 1) / 2;
        *height = (*height + 1) / 2;
    }

    while(*width > limit || *height > limit) {
        *width = (*width + 1) / 2;
        *height = (*height + 1) / 2;
    }

    return true;
}

static unsigned __be16(const unsigned char* data)
{
    return (unsigned)data[0] << 8 | data[1];
}

static uint32_t __be32(const unsigned char* data)
{
    return (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 | (uint32_t)data[2] << 8 | data[3];
}

static uint32_t __crc32(const unsigned char* data, size_t size)
{
    uint32_t crc = UINT32_MAX;
    while(size--) {
        crc ^= *data++;
        for(unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0u - (crc & 1)));
        }
    }

    return ~crc;
}

bool vita_texture_source_size(const unsigned char* data, size_t length,
    unsigned packed_width, unsigned packed_height, unsigned* width, unsigned* height)
{
    if(!data || !width || !height) {
        return false;
    }

    const unsigned char* record;
    if(length >= 57 && !memcmp(data, "\x89PNG\r\n\x1a\n", 8)) {
        if(__be32(data + 8) != 13 || memcmp(data + 12, "IHDR", 4) ||
            __be32(data + 16) != packed_width || __be32(data + 20) != packed_height ||
            __be32(data + 33) != 12 || memcmp(data + 37, "vaRc", 4) ||
            __be32(data + 53) != __crc32(data + 37, 16)) {
            return false;
        }

        record = data + 41;
    } else if(length >= 18 && !memcmp(data, "\xff\xd8\xff\xef\0\x0e", 6)) {
        record = data + 6;
    } else {
        return false;
    }

    if(memcmp(record, "VTX1", 4)) {
        return false;
    }

    unsigned source_width = __be16(record + 4);
    unsigned source_height = __be16(record + 6);
    unsigned small_width = source_width;
    unsigned small_height = source_height;
    if(!vita_texture_small_size(&small_width, &small_height) ||
        small_width != packed_width || small_height != packed_height ||
        __be16(record + 8) != packed_width || __be16(record + 10) != packed_height ||
        (source_width == packed_width && source_height == packed_height)) {
        return false;
    }

    *width = source_width;
    *height = source_height;
    return true;
}
