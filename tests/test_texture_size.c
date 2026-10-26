#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "texture_size.h"

static void __size(unsigned width, unsigned height, unsigned expected_width, unsigned expected_height)
{
    assert(vita_texture_small_size(&width, &height));
    assert(width == expected_width && height == expected_height);
}

int main(int argc, char** argv)
{
    __size(1, 1, 1, 1);
    __size(63, 1024, 63, 1024);
    __size(64, 255, 64, 255);
    __size(64, 256, 32, 128);
    __size(1001, 1801, 501, 901);
    __size(4096, 2048, 512, 256);
    __size(1, 16384, 1, 4096);
    unsigned width = 0, height = 1;
    assert(!vita_texture_small_size(&width, &height));
    assert(!vita_texture_small_size(NULL, &height));
    unsigned char jpeg[] = {0xff, 0xd8, 0xff, 0xef, 0, 14, 'V', 'T', 'X', '1',
        3, 233, 7, 9, 1, 245, 3, 133};
    assert(vita_texture_source_size(jpeg, sizeof(jpeg), 501, 901, &width, &height));
    assert(width == 1001 && height == 1801);
    assert(!vita_texture_source_size(jpeg, sizeof(jpeg), 500, 901, &width, &height));
    for(size_t size = 0; size < sizeof(jpeg); ++size) {
        assert(!vita_texture_source_size(jpeg, size, 501, 901, &width, &height));
    }

    jpeg[11] = 0;
    assert(!vita_texture_source_size(jpeg, sizeof(jpeg), 501, 901, &width, &height));
    /* Optional cross-language fixture produced by the Python resampler. */
    if(argc == 2) {
        FILE* stream = fopen(argv[1], "rb");
        assert(stream);
        unsigned char header[57];
        size_t count = fread(header, 1, sizeof(header), stream);
        fclose(stream);
        assert(vita_texture_source_size(header, count, 501, 901, &width, &height));
        assert(width == 1001 && height == 1801);
        header[!memcmp(header, "\x89PNG", 4) ? 45 : 10] ^= 1;
        assert(!vita_texture_source_size(header, count, 501, 901, &width, &height));
    }

    puts("Texture thresholds, odd dimensions, metadata validation and truncated inputs passed.");
    return 0;
}
