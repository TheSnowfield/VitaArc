#ifndef VITAARC_TEXTURE_SIZE_H
#define VITAARC_TEXTURE_SIZE_H

#include <stdbool.h>
#include <stddef.h>

/* Same integer rounding and thresholds as tools/package_textures.py. */
bool vita_texture_small_size(unsigned* width, unsigned* height);

/* Inspect the first 57 bytes of PNG or 18 bytes of JPEG, without decoding it.
 * Dimensions are restored only for tagged, consistently resampled images. */
bool vita_texture_source_size(const unsigned char* data, size_t length,
    unsigned packed_width, unsigned packed_height, unsigned* width, unsigned* height);

#endif
