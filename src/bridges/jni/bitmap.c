/* Native implementation of the Cocos Java bitmap service. Layout uses the
 * packaged font through FreeType; Java objects and game callbacks stay in JNI.
 * The APK's Java implementation accepts but does not use its shadow arguments. */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H

#include <config.h>
#include <logcat/logcat.h>
#include "bitmap.h"
#include "jni.h"

#define BITMAP_SIDE 2048
#define BITMAP_TEXT_MAX 65536

typedef struct glyph_place {
    uint32_t code;
    FT_UInt glyph;
    int x, row;
} glyph_place_t;

static uint32_t next_utf8(const uint8_t* text, size_t length, size_t* offset)
{
    uint32_t first = text[(*offset)++];
    if(first < 128)
        return first;

    unsigned tail = first >= 0xc2 && first <= 0xdf ? 1
        : first >= 0xe0 && first <= 0xef           ? 2
        : first >= 0xf0 && first <= 0xf4           ? 3
                                                   : 0;
    if(!tail || tail > length - *offset)
        return 0xfffd;

    uint32_t code = first & (0x7f >> tail);
    for(unsigned index = 0; index < tail; ++index) {
        uint8_t byte = text[*offset + index];
        if((byte & 0xc0) != 0x80)
            return 0xfffd;

        code = (code << 6) | (byte & 63);
    }

    if((tail == 1 && code < 0x80) || (tail == 2 && code < 0x800) || (tail == 3 && code < 0x10000) || code > 0x10ffff ||
        (code >= 0xd800 && code <= 0xdfff))
        return 0xfffd;

    *offset += tail;
    return code;
}

static bool layout(
    FT_Face face, glyph_place_t* places, size_t count, int bound, bool wrap, int* widths, int* rows, int* max_width)
{
    int row = 0, pen = 0;
    size_t row_start = 0, break_at = 0;
    int break_width = 0;
    FT_UInt previous = 0;
    *max_width = 0;
    for(size_t index = 0; index < count; ++index) {
        glyph_place_t* place = &places[index];
        place->row = -1;
        if(place->code == '\r')
            continue;

        if(place->code == '\n') {
            widths[row++] = pen;
            pen = 0;
            previous = 0;
            row_start = index + 1;
            break_at = 0;
            continue;
        }

        place->glyph = FT_Get_Char_Index(face, place->code);
        if(FT_Load_Glyph(face, place->glyph, FT_LOAD_DEFAULT))
            return false;

        FT_Vector kerning = {0, 0};

        if(previous && place->glyph && FT_HAS_KERNING(face))
            FT_Get_Kerning(face, previous, place->glyph, FT_KERNING_DEFAULT, &kerning);
        int kern = (int)(kerning.x / 64);
        int advance = (int)((face->glyph->advance.x + 32) / 64);
        if(wrap && bound > 0 && pen > 0 && pen + kern + advance > bound) {
            if(break_at > row_start && break_at < index) {
                widths[row++] = break_width;
                index = break_at - 1;
                row_start = break_at;
            } else {
                widths[row++] = pen;
                row_start = index;
                --index;
            }

            pen = 0;
            previous = 0;
            break_at = 0;
            continue;
        }

        place->x = pen + kern;
        place->row = row;
        if(place->code == ' ' || place->code == '\t') {
            break_at = index + 1;
            break_width = pen;
        }

        pen += kern + advance;
        previous = place->glyph;
    }

    widths[row] = pen;
    *rows = row + 1;
    for(int index = 0; index <= row; ++index)
        if(widths[index] > *max_width)
            *max_width = widths[index];
    return true;
}

static void composite(uint8_t* destination, unsigned coverage, const uint8_t color[4])
{
    unsigned alpha = (coverage * color[3] + 127) / 255;
    for(unsigned component = 0; component < 3; ++component)
        destination[component] =
            (uint8_t)((color[component] * alpha + destination[component] * (255 - alpha) + 127) / 255);
    destination[3] = (uint8_t)(alpha + (destination[3] * (255 - alpha) + 127) / 255);
}

static void draw(bridge_bitmap_t* output, const FT_Bitmap* bitmap, int x, int y, const uint8_t color[4])
{
    for(unsigned row = 0; row < bitmap->rows; ++row) {
        int dy = y + (int)row;
        if(dy < 0 || dy >= output->height)
            continue;

        const uint8_t* source = bitmap->buffer +
            (bitmap->pitch >= 0 ? (size_t)row * bitmap->pitch : (size_t)(bitmap->rows - row - 1) * -bitmap->pitch);
        for(unsigned column = 0; column < bitmap->width; ++column) {
            int dx = x + (int)column;
            if(dx < 0 || dx >= output->width)
                continue;

            unsigned coverage = bitmap->pixel_mode == FT_PIXEL_MODE_MONO
                ? (source[column / 8] & (128 >> (column % 8)) ? 255 : 0)
                : source[column];
            composite(output->rgba + ((size_t)dy * output->width + dx) * 4, coverage, color);
        }
    }
}

bool bridge_bitmap_render(const bridge_bitmap_request_t* request, bridge_bitmap_t* output)
{
    if(!output)
        return false;

    memset(output, 0, sizeof(*output));
    if(!request || !request->text || !request->text_size || request->text_size > BITMAP_TEXT_MAX ||
        !request->font_path || request->font_size < 1 || request->font_size > 512 || request->width < 0 ||
        request->width > BITMAP_SIDE || request->height < 0 || request->height > BITMAP_SIDE || request->overflow < 0 ||
        request->overflow > 2 ||
        (request->stroke && (!isfinite(request->stroke_size) || request->stroke_size < 0 || request->stroke_size > 64)))
        return false;

    bool success = false;
    FT_Library library = NULL;
    FT_Face face = NULL;
    FT_Stroker stroker = NULL;
    glyph_place_t* places = calloc(request->text_size, sizeof(*places));
    int* widths = calloc(request->text_size + 1, sizeof(*widths));
    if(!places || !widths || FT_Init_FreeType(&library) || FT_New_Face(library, request->font_path, 0, &face))
        goto done;

    size_t count = 0, offset = 0;
    while(offset < request->text_size)
        places[count++].code = next_utf8(request->text, request->text_size, &offset);
    int rows = 0, measured_width = 0, line_height = 0, ascender = 0;
    int size = request->font_size;
    bool shrink = request->overflow == 2 && request->width > 0 && request->height > 0;
    bool wrap = request->width > 0 && !(request->overflow == 1 && !request->wrap) && !(shrink && !request->wrap);
    for(;;) {
        if(FT_Set_Pixel_Sizes(face, 0, size) ||
            !layout(face, places, count, request->width, wrap, widths, &rows, &measured_width))
            goto done;

        ascender = (int)((face->size->metrics.ascender + 63) / 64);
        line_height = (int)((face->size->metrics.height + 63) / 64);
        if(line_height < 1)
            line_height = size;
        if(!shrink || size == 1 || (measured_width <= request->width && rows * line_height <= request->height))
            break;

        --size;
    }

    output->width = request->width > 0 ? request->width : measured_width;
    output->height = request->height > 0 ? request->height : rows * line_height;
    if(output->width < 1 || output->width > BITMAP_SIDE || output->height < 1 || output->height > BITMAP_SIDE)
        goto done;

    output->rgba = calloc((size_t)output->width * output->height, 4);
    if(!output->rgba)
        goto done;

    int vertical = (request->alignment >> 4) & 15;
    int y_offset = vertical == 2 ? output->height - rows * line_height
        : vertical == 3          ? (output->height - rows * line_height) / 2
                                 : 0;
    if(request->stroke) {
        if(FT_Stroker_New(library, &stroker))
            goto done;

        FT_Stroker_Set(stroker,
            (FT_Fixed)(request->stroke_size > 0 ? request->stroke_size * 32 : 32),
            FT_STROKER_LINECAP_ROUND,
            FT_STROKER_LINEJOIN_ROUND,
            0);
    }

    for(size_t index = 0; index < count; ++index) {
        glyph_place_t* place = &places[index];
        if(place->row < 0)
            continue;

        int horizontal = request->alignment & 15;
        int x_offset = horizontal == 2 ? output->width - widths[place->row]
            : horizontal == 3          ? (output->width - widths[place->row]) / 2
                                       : 0;
        int x = x_offset + place->x;
        int baseline = y_offset + place->row * line_height + ascender;
        if(FT_Load_Glyph(face, place->glyph, FT_LOAD_DEFAULT))
            goto done;

        if(stroker && face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
            FT_Glyph glyph = NULL;
            if(FT_Get_Glyph(face->glyph, &glyph))
                goto done;

            FT_Error error = FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1);
            if(!error)
                error = FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, NULL, 1);
            if(!error) {
                FT_BitmapGlyph bitmap = (FT_BitmapGlyph)glyph;
                draw(output, &bitmap->bitmap, x + bitmap->left, baseline - bitmap->top, request->stroke_color);
            }

            FT_Done_Glyph(glyph);
            if(error)
                goto done;
        }

        if(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL))
            goto done;

        draw(output,
            &face->glyph->bitmap,
            x + face->glyph->bitmap_left,
            baseline - face->glyph->bitmap_top,
            request->color);
    }

    success = true;
done:
    if(stroker)
        FT_Stroker_Done(stroker);
    if(face)
        FT_Done_Face(face);
    if(library)
        FT_Done_FreeType(library);
    free(places);
    free(widths);
    if(!success) {
        free(output->rgba);
        memset(output, 0, sizeof(*output));
    }

    return success;
}

bool jni_bitmap_create(JNIEnv* env, const jvalue* arguments)
{
    if(!arguments[0].l || !arguments[1].l)
        return false;

    jsize length = (*env)->GetArrayLength(env, arguments[0].l);
    if(length <= 0 || length > BITMAP_TEXT_MAX || (*env)->ExceptionCheck(env))
        return false;

    uint8_t* text = malloc((size_t)length);
    if(!text)
        return false;

    (*env)->GetByteArrayRegion(env, arguments[0].l, 0, length, (jbyte*)text);
    const char* font = (*env)->GetStringUTFChars(env, arguments[1].l, NULL);
    bool result = false;
    if(font && !(*env)->ExceptionCheck(env)) {
        char path[VITAARC_PATH_CAPACITY];
        const char* relative = font;
        size_t assets_length = strlen(PATH_TO_ASSETS);
        if(!strncmp(relative, PATH_TO_ASSETS, assets_length) && relative[assets_length] == '/')
            relative += assets_length + 1;
        if(!strncmp(relative, "assets/", 7))
            relative += 7;
        bool asset_font = (strstr(relative, ".ttf") || strstr(relative, ".otf")) && relative[0] != '/' &&
            !strstr(relative, "..") && !strchr(relative, ':');
        int written =
            snprintf(path, sizeof(path), "%s/%s", PATH_TO_ASSETS, asset_font ? relative : "Fonts/Kazesawa-Regular.ttf");
        bridge_bitmap_request_t request = {.text = text,
            .text_size = (size_t)length,
            .font_path = path,
            .font_size = arguments[2].i,
            .alignment = arguments[7].i,
            .width = arguments[8].i,
            .height = arguments[9].i,
            .stroke = arguments[15].z,
            .stroke_size = arguments[20].f,
            .wrap = arguments[21].z,
            .overflow = arguments[22].i};

        for(unsigned index = 0; index < 4; ++index) {
            request.color[index] = (uint8_t)arguments[3 + index].i;
            request.stroke_color[index] = (uint8_t)arguments[16 + index].i;
        }

        bridge_bitmap_t bitmap;
        bool rendered = written > 0 && written < (int)sizeof(path) && bridge_bitmap_render(&request, &bitmap);
        if(!rendered && asset_font) {
            written = snprintf(path, sizeof(path), "%s/Fonts/Kazesawa-Regular.ttf", PATH_TO_ASSETS);
            rendered = written > 0 && written < (int)sizeof(path) && bridge_bitmap_render(&request, &bitmap);
        }

        if(rendered) {
            result = bridge_jni_cocos_publish_bitmap(bitmap.width, bitmap.height, bitmap.rgba);
            free(bitmap.rgba);
        } else {
            log_e(TAG,
                "Text bitmap failed: font=%s size=%d dimensions=%dx%d",
                font,
                request.font_size,
                request.width,
                request.height);
        }
    }

    if(font)
        (*env)->ReleaseStringUTFChars(env, arguments[1].l, font);
    free(text);
    return result;
}
