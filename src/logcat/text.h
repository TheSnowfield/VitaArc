#ifndef VITAARC_LOG_TEXT_H
#define VITAARC_LOG_TEXT_H

#include <stdbool.h>
#include <stdint.h>
#include "logcat.h"

#define LOG_ROWS 31
#define LOG_COLS 78

typedef struct {
    const uint32_t* pixels;
    unsigned width;
    unsigned height;
} log_image_t;

typedef struct {
    char text[LOG_COLS + 1];
    log_level_t level;
    unsigned image;
} log_line_t;

typedef struct {
    log_line_t lines[LOG_ROWS];
    unsigned row;
    unsigned col;
    unsigned count;
    bool newline;
    const log_image_t* image;
} log_text_t;

void log_text_image(log_text_t* text, const log_image_t* image);

void log_text_write(log_text_t* text, log_level_t level, const char* data, size_t size);

void log_text_draw(const log_text_t* text, uint32_t* pixels, unsigned width, unsigned height, unsigned pitch);

#endif
