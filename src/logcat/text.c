#include <string.h>
#include "text.h"
#include "font.h"

static void __next(log_text_t* text)
{
    text->row = (text->row + 1) % LOG_ROWS;
    text->col = 0;
    if(text->count < LOG_ROWS) {
        ++text->count;
    }

    memset(&text->lines[text->row], 0, sizeof(log_line_t));
}

static void __put(log_text_t* text, log_level_t level, char value)
{
    if(text->col == LOG_COLS) {
        __next(text);
    }

    log_line_t* line = &text->lines[text->row];
    if(level > line->level) {
        line->level = level;
    }

    line->text[text->col++] = value;
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
                __next(text);
                break;

            case '\r':
                text->col = 0;
                break;

            case '\t': {
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

    unsigned first = (text->row + LOG_ROWS + 1 - text->count) % LOG_ROWS;
    for(unsigned row = 0; row < text->count; ++row) {
        unsigned y = (20 + row * 8) * scale;
        if(y + 7 * scale > height) {
            break;
        }

        const log_line_t* line = &text->lines[(first + row) % LOG_ROWS];
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
