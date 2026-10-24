#include <png.h>
#include <stdlib.h>

#include "logo.h"
#include "generated/boot_logo.h"

#define LOGO_WIDTH 120u
#define LOGO_HEIGHT 64u

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

const log_image_t* log_logo_open(void)
{
    if(__image.pixels) {
        return &__image;
    }

    png_image source = {.version = PNG_IMAGE_VERSION};
    if(!png_image_begin_read_from_memory(&source, __logo_png, sizeof(__logo_png))) {
        png_image_free(&source);
        return NULL;
    }

    source.format = PNG_FORMAT_RGBA;
    unsigned char* pixels = malloc(PNG_IMAGE_SIZE(source));
    uint32_t* small = malloc(LOGO_WIDTH * LOGO_HEIGHT * sizeof(*small));
    if(!pixels || !small || !png_image_finish_read(&source, NULL, pixels, 0, NULL)) {
        free(pixels);
        free(small);
        png_image_free(&source);
        return NULL;
    }

    for(unsigned y = 0; y < LOGO_HEIGHT; ++y) {
        unsigned row = y * source.height / LOGO_HEIGHT;
        for(unsigned x = 0; x < LOGO_WIDTH; ++x) {
            unsigned col = x * source.width / LOGO_WIDTH;
            unsigned char* pixel = (unsigned char*)&small[y * LOGO_WIDTH + x];
            const unsigned char* input = pixels + (row * source.width + col) * 4;
            for(unsigned channel = 0; channel < 4; ++channel) {
                pixel[channel] = input[channel];
            }

            __matte(pixel);
        }
    }

    free(pixels);
    png_image_free(&source);
    __image = (log_image_t){small, LOGO_WIDTH, LOGO_HEIGHT};
    return &__image;
}

void log_logo_close(void)
{
    free((void*)__image.pixels);
    __image = (log_image_t){0};
}
