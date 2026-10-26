#include <stdio.h>
#include <string.h>
#include <config.h>
#include "text.h"
#include "font.h"

#define LOG_BACKGROUND 0xff18120cu

static void __next(log_text_t* text)
{
    text->row = (text->row + 1) % LOG_ROWS;
    text->col = 0;
    text->newline = false;
    if(text->count < LOG_ROWS) {
        ++text->count;
    }

    memset(&text->lines[text->row], 0, sizeof(log_line_t));
}

static void __put(log_text_t* text, log_level_t level, char value)
{
    if(text->newline || text->col == LOG_COLS) {
        __next(text);
    }

    log_line_t* line = &text->lines[text->row];
    if(level > line->level) {
        line->level = level;
    }

    line->text[text->col++] = value;
}

void log_text_image(log_text_t* text, const log_image_t* image)
{
    if(!text || !image || !image->pixels || !image->width || !image->height) {
        return;
    }

    text->image = image;
}

void log_text_write(log_text_t* text, log_level_t level, const char* data, size_t size)
{
    if(!text || !data) {
        return;
    }

    if(!text->count) {
        text->count = 1;
    }

    for(size_t index = 0; index < size; ++index) {
        unsigned char value = (unsigned char)data[index];
        switch(value) {
            case '\n':
                if(text->newline) {
                    __next(text);
                }

                text->newline = true;
                break;

            case '\r':
                if(text->newline) {
                    __next(text);
                }

                text->col = 0;
                break;

            case '\t': {
                if(text->newline) {
                    __next(text);
                }

                unsigned spaces = 4 - text->col % 4;
                while(spaces--) {
                    __put(text, level, ' ');
                }

                break;
            }

            default:
                if(value >= ' ') {
                    __put(text, level, value <= '~' ? (char)value : '?');
                }

                break;
        }
    }
}

static void __glyph(uint32_t* pixels, unsigned pitch, unsigned x, unsigned y, unsigned scale, char value, uint32_t color)
{
    const uint8_t* glyph = __font[(unsigned char)value - ' '];
    for(unsigned row = 0; row < 7; ++row) {
        for(unsigned col = 0; col < 5; ++col) {
            if(!(glyph[row] & (16u >> col))) {
                continue;
            }

            for(unsigned dy = 0; dy < scale; ++dy) {
                uint32_t* pixel = pixels + (y + row * scale + dy) * pitch + x + col * scale;
                for(unsigned dx = 0; dx < scale; ++dx) {
                    pixel[dx] = color;
                }
            }
        }
    }
}

static uint32_t __color(log_level_t level)
{
    switch(level) {
        case WARNING: return 0xff70d8ffu;
        case ERROR:
        case FATAL:   return 0xff8080ffu;
        case VERBOSE: return 0xffa0a0a0u;
        default:      return 0xffe8e8e8u;
    }
}

static uint32_t __blend(uint32_t background, uint32_t foreground)
{
    unsigned alpha = foreground >> 24;
    if(alpha == 255) {
        return foreground;
    }

    unsigned char* target = (void*)&background;
    const unsigned char* source = (const void*)&foreground;
    for(unsigned channel = 0; channel < 3; ++channel) {
        target[channel] = (source[channel] * alpha + target[channel] * (255 - alpha)) / 255;
    }

    return background;
}

static void __block(uint32_t* pixels, unsigned pitch, unsigned x, unsigned y, unsigned scale, uint32_t color)
{
    for(unsigned dy = 0; dy < scale; ++dy) {
        uint32_t* target = pixels + (y + dy) * pitch + x;
        for(unsigned dx = 0; dx < scale; ++dx) {
            target[dx] = color;
        }
    }
}

static void __image(const log_image_t* image, uint32_t* pixels,
    unsigned width, unsigned height, unsigned pitch, unsigned y, unsigned scale)
{
    if(!image || !image->pixels) {
        return;
    }

    unsigned rows = y < height ? (height - y) / scale : 0;
    if(rows > image->height) {
        rows = image->height;
    }

    unsigned cols = width / scale > 6 ? width / scale - 6 : 0;
    if(cols > image->width) {
        cols = image->width;
    }

    for(unsigned row = 0; row < rows; ++row) {
        for(unsigned col = 0; col < cols; ++col) {
            unsigned x = (6 + col) * scale;
            unsigned top = y + row * scale;
            uint32_t source = image->pixels[row * image->width + col];
            if(!(source >> 24)) {
                continue;
            }

            uint32_t color = __blend(pixels[top * pitch + x], source);
            __block(pixels, pitch, x, top, scale, color);
        }
    }
}

static unsigned __content_top(const log_text_t* text, unsigned height, unsigned scale)
{
    unsigned top = 20 * scale;
    if(text->image) {
        unsigned available = height > top ? (height - top) / scale : 0;
        unsigned rows = text->image->height < available ? text->image->height : available;
        top += (rows + 8) * scale;
    }

    return top < height ? top : height;
}

static void __line(uint32_t* pixels, unsigned width, unsigned height, unsigned pitch,
    unsigned x, unsigned y, unsigned scale, const char* line, uint32_t color)
{
    if(y + 7 * scale > height) {
        return;
    }

    for(unsigned col = 0; line[col] && x + 5 * scale <= width; ++col, x += 6 * scale) {
        unsigned char value = (unsigned char)line[col];
        __glyph(pixels, pitch, x, y, scale, value >= ' ' && value <= '~' ? (char)value : '?', color);
    }
}

void log_text_draw(const log_text_t* text, uint32_t* pixels, unsigned width, unsigned height, unsigned pitch)
{
    if(!text || !pixels || width < 32 || height < 32 || pitch < width) {
        return;
    }

    unsigned scale = width >= 960 && height >= 544 ? 2 : 1;
    for(unsigned y = 0; y < height; ++y) {
        for(unsigned x = 0; x < width; ++x) {
            pixels[y * pitch + x] = LOG_BACKGROUND;
        }
    }

    __line(pixels, width, height, pitch, 6 * scale, 6 * scale, scale, "VitaArc - startup", 0xffffd080u);
    if(text->image) {
        __image(text->image, pixels, width, height, pitch, 20 * scale, scale);
    }

    unsigned top = __content_top(text, height, scale);
    unsigned count = (height - top) / (8 * scale);
    if(count > text->count) {
        count = text->count;
    }

    unsigned first = (text->row + LOG_ROWS + 1 - count) % LOG_ROWS;
    for(unsigned row = 0; row < count; ++row) {
        unsigned y = top + row * 8 * scale;
        if(y + 7 * scale > height) {
            break;
        }

        const log_line_t* line = &text->lines[(first + row) % LOG_ROWS];
        __line(pixels, width, height, pitch, 6 * scale, y, scale, line->text, __color(line->level));
    }
}

void log_text_draw_menu(const log_text_t* text, const char* const* names, unsigned count, unsigned selected,
    uint32_t* pixels, unsigned width, unsigned height, unsigned pitch)
{
    if(!text || !pixels || width < 32 || height < 32 || pitch < width) {
        return;
    }

    unsigned scale = width >= 960 && height >= 544 ? 2 : 1;
    unsigned top = __content_top(text, height, scale);
    for(unsigned y = top; y < height; ++y) {
        for(unsigned x = 0; x < width; ++x) {
            pixels[y * pitch + x] = LOG_BACKGROUND;
        }
    }

    unsigned x = 6 * scale;
    unsigned row_height = 8 * scale;
    if(!names || !count) {
        __line(pixels, width, height, pitch, x, top, scale, "No Arcaea versions installed.", 0xff70d8ffu);
        __line(pixels, width, height, pitch, x, top + 2 * row_height, scale,
            "Copy the prepared version directory to:", 0xffe8e8e8u);
        __line(pixels, width, height, pitch, x, top + 3 * row_height, scale,
            PATH_TO_VERSIONS "<version>/", 0xffe8e8e8u);
        __line(pixels, width, height, pitch, x, top + 5 * row_height, scale, "Circle: exit", 0xffffd080u);
        return;
    }

    if(selected >= count) {
        selected = 0;
    }

    char title[64];
    snprintf(title, sizeof(title), "Select Arcaea version (%u/%u)", selected + 1, count);
    __line(pixels, width, height, pitch, x, top, scale, title, 0xffffd080u);
    unsigned rows = (height - top) / row_height;
    unsigned visible = rows > 5 ? rows - 5 : 1;
    unsigned first = selected / visible * visible;
    for(unsigned row = 0; row < visible && first + row < count; ++row) {
        unsigned index = first + row;
        unsigned y = top + (row + 2) * row_height;
        if(index == selected) {
            __line(pixels, width, height, pitch, x, y, scale, ">", 0xffffd080u);
        }

        __line(pixels, width, height, pitch, x + 2 * 6 * scale, y, scale,
            names[index] ? names[index] : "?", index == selected ? 0xffffd080u : 0xffe8e8e8u);
    }

    __line(pixels, width, height, pitch, x, top + (visible + 3) * row_height, scale,
        "Up/Down: select   Cross: boot   Circle: exit", 0xffa0a0a0u);
}
