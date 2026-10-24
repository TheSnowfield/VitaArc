#include <string.h>
#include "text.h"
#include "font.h"

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
    unsigned rows = (image->height + 7) / 8;
    if(rows > LOG_ROWS) {
        rows = LOG_ROWS;
    }

    for(unsigned row = 0; row < rows; ++row) {
        if(text->count) {
            __next(text);
        } else {
            text->count = 1;
        }

        text->lines[text->row].image = row + 1;
    }

    __next(text);
    text->newline = true;
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

static void __image(const log_image_t* image, unsigned slice, uint32_t* pixels,
    unsigned width, unsigned height, unsigned pitch, unsigned y, unsigned scale)
{
    if(!image || !image->pixels) {
        return;
    }

    unsigned first = (slice - 1) * 8;
    for(unsigned row = 0; row < 8 && first + row < image->height; ++row) {
        for(unsigned col = 0; col < image->width; ++col) {
            unsigned x = (6 + col) * scale;
            unsigned top = y + row * scale;
            if(x + scale > width || top + scale > height) {
                continue;
            }

            uint32_t source = image->pixels[(first + row) * image->width + col];
            if(!(source >> 24)) {
                continue;
            }

            uint32_t color = __blend(pixels[top * pitch + x], source);
            __block(pixels, pitch, x, top, scale, color);
        }
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
            pixels[y * pitch + x] = 0xff18120cu;
        }
    }

    const char title[] = "VitaArc - startup";
    for(unsigned col = 0; title[col] && (col * 6 + 11) * scale <= width; ++col) {
        __glyph(pixels, pitch, (col * 6 + 6) * scale, 6 * scale, scale, title[col], 0xffffd080u);
    }

    unsigned count = (height - 19 * scale) / (8 * scale);
    if(count > text->count) {
        count = text->count;
    }

    unsigned first = (text->row + LOG_ROWS + 1 - count) % LOG_ROWS;
    for(unsigned row = 0; row < count; ++row) {
        unsigned y = (20 + row * 8) * scale;
        if(y + 7 * scale > height) {
            break;
        }

        const log_line_t* line = &text->lines[(first + row) % LOG_ROWS];
        if(line->image) {
            __image(text->image, line->image, pixels, width, height, pitch, y, scale);
            continue;
        }

        uint32_t color = __color(line->level);
        for(unsigned col = 0; col < LOG_COLS && line->text[col]; ++col) {
            unsigned x = (6 + col * 6) * scale;
            if(x + 5 * scale > width) {
                break;
            }

            __glyph(pixels, pitch, x, y, scale, line->text[col], color);
        }
    }
}
