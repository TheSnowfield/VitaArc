#include <png.h>
#include <stdlib.h>
#include <string.h>

#include "logo.h"
#include "generated/boot_logo.h"
#include "generated/boot_icon.h"

#define LOGO_HEIGHT 64u
#define LOGO_GAP 12u

static log_image_t __image;

/**
 * @brief Remove the white matte while preserving the logo's edge coverage.
 */
static void __matte(unsigned char* pixel)
{
    unsigned white = pixel[0];
    if(pixel[1] < white) {
        white = pixel[1];
    }

    if(pixel[2] < white) {
        white = pixel[2];
    }

    unsigned coverage = 255 - white;
    pixel[3] = pixel[3] * coverage / 255;
    if(!coverage) {
        return;
    }

    for(unsigned channel = 0; channel < 3; ++channel) {
        pixel[channel] = (pixel[channel] - white) * 255 / coverage;
    }
}

static log_image_t __load(const unsigned char* data, size_t size, bool matte, unsigned height)
{
    png_image source = {.version = PNG_IMAGE_VERSION};
    if(!png_image_begin_read_from_memory(&source, data, size)) {
        png_image_free(&source);
        return (log_image_t){0};
    }

    source.format = PNG_FORMAT_RGBA;
    unsigned char* pixels = malloc(PNG_IMAGE_SIZE(source));
    if(!pixels || !png_image_finish_read(&source, NULL, pixels, 0, NULL)) {
        free(pixels);
        png_image_free(&source);
        return (log_image_t){0};
    }

    /* Trim transparent padding before applying the requested visible height. */
    unsigned left = source.width, top = source.height, right = 0, bottom = 0;
    for(unsigned y = 0; y < source.height; ++y) {
        for(unsigned x = 0; x < source.width; ++x) {
            unsigned char* pixel = pixels + (y * source.width + x) * 4;
            if(matte) {
                __matte(pixel);
            }

            if(pixel[3]) {
                if(x < left) left = x;
                if(y < top) top = y;
                if(x + 1 > right) right = x + 1;
                if(y + 1 > bottom) bottom = y + 1;
            }
        }
    }

    if(left >= right || top >= bottom) {
        free(pixels);
        png_image_free(&source);
        return (log_image_t){0};
    }

    /* A zero target height keeps the icon's native pixel grid unchanged. */
    if(!height) height = bottom - top;
    unsigned width = ((right - left) * height + (bottom - top) / 2) / (bottom - top);
    if(!width) width = 1;
    uint32_t* small = malloc(width * height * sizeof(*small));
    if(!small) {
        free(pixels);
        png_image_free(&source);
        return (log_image_t){0};
    }

    /* Nearest-neighbor sampling keeps the new icon's pixel edges sharp. */
    for(unsigned y = 0; y < height; ++y) {
        unsigned row = top + y * (bottom - top) / height;
        for(unsigned x = 0; x < width; ++x) {
            unsigned col = left + x * (right - left) / width;
            const unsigned char* input = pixels + (row * source.width + col) * 4;
            memcpy(&small[y * width + x], input, 4);
        }
    }

    free(pixels);
    png_image_free(&source);
    return (log_image_t){small, width, height};
}

const log_image_t* log_logo_open(void)
{
    if(__image.pixels) {
        return &__image;
    }

    log_image_t icon = __load(__icon_png, sizeof(__icon_png), false, 0);
    unsigned gl_height = icon.pixels ? icon.height - icon.height / 10 : LOGO_HEIGHT;
    log_image_t gl = __load(__logo_png, sizeof(__logo_png), true, gl_height);
    if(!gl.pixels || !icon.pixels) {
        __image = gl.pixels ? gl : icon;
        return __image.pixels ? &__image : NULL;
    }

    unsigned width = gl.width + LOGO_GAP + icon.width;
    unsigned height = icon.height;
    uint32_t* pixels = calloc(width * height, sizeof(*pixels));
    if(pixels) {
        for(unsigned y = 0; y < height; ++y) {
            memcpy(pixels + y * width, icon.pixels + y * icon.width, icon.width * sizeof(*pixels));
        }

        unsigned top = (height - gl.height) / 2;
        for(unsigned y = 0; y < gl.height; ++y) {
            memcpy(pixels + (top + y) * width + icon.width + LOGO_GAP,
                gl.pixels + y * gl.width, gl.width * sizeof(*pixels));
        }
    }

    free((void*)gl.pixels);
    free((void*)icon.pixels);
    if(!pixels) {
        return NULL;
    }

    __image = (log_image_t){pixels, width, height};
    return &__image;
}

void log_logo_close(void)
{
    free((void*)__image.pixels);
    __image = (log_image_t){0};
}
