#include <assert.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <logcat/logo.h>
#include "generated/boot_icon.h"

static void __layout(const log_image_t* image)
{
    png_image source = {.version = PNG_IMAGE_VERSION};
    assert(png_image_begin_read_from_memory(&source, __icon_png, sizeof(__icon_png)));
    source.format = PNG_FORMAT_RGBA;
    uint32_t* expected = malloc(PNG_IMAGE_SIZE(source));
    assert(expected && png_image_finish_read(&source, NULL, expected, 0, NULL));
    assert(image && image->pixels && image->height == source.height);
    /* Preserve either the local pixel icon or the tracked fallback unchanged. */
    unsigned gl_height = source.height - source.height / 10;
    unsigned gl_width = (128 * gl_height + 29) / 58;
    unsigned gl_top = (source.height - gl_height) / 2;
    const unsigned start[2] = {0, source.width + 12};
    const unsigned end[2] = {source.width, start[1] + gl_width};
    const unsigned top[2] = {0, gl_top}, bottom[2] = {source.height, gl_top + gl_height};
    assert(image->width == end[1]);
    for(unsigned y = 0; y < image->height; ++y) {
        for(unsigned x = end[0]; x < start[1]; ++x) {
            assert(!(image->pixels[y * image->width + x] >> 24));
        }
    }

    for(unsigned part = 0; part < 2; ++part) {
        bool first = false, last = false;
        for(unsigned x = start[part]; x < end[part]; ++x) {
            first |= (image->pixels[top[part] * image->width + x] >> 24) != 0;
            last |= (image->pixels[(bottom[part] - 1) * image->width + x] >> 24) != 0;
            for(unsigned y = 0; y < image->height; ++y) {
                if(y < top[part] || y >= bottom[part]) {
                    assert(!(image->pixels[y * image->width + x] >> 24));
                }
            }
        }

        assert(first && last);
    }

    unsigned white = 0;
    for(unsigned y = 0; y < image->height; ++y) {
        for(unsigned x = start[0]; x < end[0]; ++x) {
            const unsigned char* pixel = (const void*)&image->pixels[y * image->width + x];
            white += pixel[0] > 220 && pixel[1] > 210 && pixel[2] > 210 && pixel[3] > 240;
        }
    }

    /* The icon's white hair and hat must remain opaque after vitaGL matting. */
    assert(white > 100);

    assert(source.width == end[0] && source.height == image->height);
    for(unsigned y = 0; y < source.height; ++y) {
        /* Every icon pixel, including its alpha, must survive byte for byte. */
        assert(!memcmp(image->pixels + y * image->width, expected + y * source.width,
            source.width * sizeof(*expected)));
    }

    free(expected);
    png_image_free(&source);
}

static void __draw(const log_image_t* image, unsigned width, unsigned height, const char* output)
{
    const uint32_t guard = 0x12345678;
    unsigned pitch = width + 16;
    size_t size = (size_t)pitch * height;
    uint32_t* storage = malloc((size + 2) * sizeof(*storage));
    assert(storage);
    for(size_t index = 0; index < size + 2; ++index) {
        storage[index] = guard;
    }

    log_text_t text = {0};
    log_text_image(&text, image);
    const char message[] = "[INFO] Preparing Arcaea runtime...\n[INFO] Startup log ready.\n";
    log_text_write(&text, INFORMATION, message, sizeof(message) - 1);
    log_text_draw(&text, storage + 1, width, height, pitch);
    assert(storage[0] == guard && storage[size + 1] == guard);
    for(unsigned y = 0; y < height; ++y) {
        for(unsigned x = width; x < pitch; ++x) {
            assert(storage[1 + y * pitch + x] == guard);
        }
    }

    if(output) {
        png_image frame = {.version = PNG_IMAGE_VERSION, .width = width,
            .height = height, .format = PNG_FORMAT_RGBA};
        assert(png_image_write_to_file(&frame, output, 0, storage + 1, (png_int_32)pitch * 4, NULL));
        png_image_free(&frame);
    }

    free(storage);
}

int main(int argc, char** argv)
{
    const log_image_t* image = log_logo_open();
    __layout(image);
    const uint32_t* pixels = image->pixels;
    assert(log_logo_open() == image && image->pixels == pixels);
    __draw(image, 480, 272, NULL);
    __draw(image, 960, 544, argc > 1 ? argv[1] : NULL);
    __draw(image, 40, 40, NULL);
    log_logo_close();
    assert(!image->pixels);
    __layout(log_logo_open());
    log_logo_close();
    log_logo_close();
    puts("Startup logos: aligned visible bounds, aspect ratios, white hair, caching and framebuffer bounds passed.");
    return 0;
}
