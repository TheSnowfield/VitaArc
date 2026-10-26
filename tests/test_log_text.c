#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <logcat/text.h>

static void __wrap(void)
{
    log_text_t text = {0};

    char line[LOG_COLS];
    memset(line, 'x', sizeof(line));
    log_text_write(&text, INFORMATION, line, sizeof(line));
    assert(text.row == 0 && text.col == LOG_COLS);
    assert(strlen(text.lines[0].text) == LOG_COLS);
    log_text_write(&text, WARNING, "!\n", 2);
    assert(text.row == 1 && text.col == 1 && text.count == 2 && text.newline);
    assert(!strcmp(text.lines[1].text, "!"));
    assert(text.lines[1].level == WARNING);

    memset(&text, 0, sizeof(text));
    log_text_write(&text, INFORMATION, line, sizeof(line));
    log_text_write(&text, INFORMATION, "\n", 1);
    assert(text.row == 0 && text.count == 1 && text.newline);
}

static void __scroll(void)
{
    log_text_t text = {0};

    for(unsigned index = 0; index < LOG_ROWS + 5; ++index) {
        char line[32];
        int size = snprintf(line, sizeof(line), "line %u\n", index);
        log_text_write(&text, INFORMATION, line, (size_t)size);
    }

    assert(text.count == LOG_ROWS && text.newline);
    unsigned first = (text.row + 1) % LOG_ROWS;
    assert(!strcmp(text.lines[first].text, "line 5"));
    assert(text.lines[text.row].text[0]);
}

static void __fragments(void)
{
    log_text_t text = {0};

    log_text_write(&text, INFORMATION, "abc", 3);
    log_text_write(&text, ERROR, "\rX\tY", 4);
    assert(!strcmp(text.lines[0].text, "X   Y"));
    assert(text.lines[0].level == ERROR);
    log_text_write(&text, INFORMATION, "\n\1\177\n", 4);
    assert(!strcmp(text.lines[1].text, "?"));
}

static void __pixels(unsigned width, unsigned height, unsigned pitch)
{
    const uint32_t guard = 0x12345678;
    size_t count = (size_t)pitch * height;
    uint32_t* storage = malloc((count + 2) * sizeof(*storage));
    assert(storage);
    for(size_t index = 0; index < count + 2; ++index) {
        storage[index] = guard;
    }

    log_text_t text = {0};

    log_text_write(&text, ERROR, "A", 1);
    log_text_draw(&text, storage + 1, width, height, pitch);
    assert(storage[0] == guard && storage[count + 1] == guard);
    unsigned scale = width >= 960 && height >= 544 ? 2 : 1;
    assert(storage[1 + 20 * scale * pitch + 7 * scale] == 0xff8080ffu);
    assert(storage[1 + 20 * scale * pitch + 6 * scale] == 0xff18120cu);

    const char* names[] = {"1.0", "2.0"};
    log_text_draw_menu(&text, names, 2, 1, storage + 1, width, height, pitch);
    log_text_draw_menu(&text, NULL, 0, 0, storage + 1, width, height, pitch);
    assert(storage[0] == guard && storage[count + 1] == guard);
    for(unsigned y = 0; y < height; ++y) {
        for(unsigned x = width; x < pitch; ++x) {
            assert(storage[1 + y * pitch + x] == guard);
        }
    }

    free(storage);
}

static void __fixed_header_and_menu(void)
{
    const unsigned width = 480, height = 272, top = 32;
    uint32_t logo_pixels[16];
    for(unsigned index = 0; index < 16; ++index) {
        logo_pixels[index] = 0xffabcdefu;
    }

    log_image_t image = {logo_pixels, 4, 4};
    log_text_t text = {0};
    log_text_image(&text, &image);
    assert(text.count == 0 && text.image == &image);
    log_text_write(&text, INFORMATION, "A\n", 2);

    uint32_t* before = calloc((size_t)width * height, sizeof(*before));
    uint32_t* after = calloc((size_t)width * height, sizeof(*after));
    assert(before && after);
    log_text_draw(&text, before, width, height, width);
    assert(before[20 * width + 6] == logo_pixels[0]);
    for(unsigned index = 0; index < LOG_ROWS + 10; ++index) {
        log_text_write(&text, ERROR, "B\n", 2);
    }

    log_text_draw(&text, after, width, height, width);
    assert(!memcmp(before, after, (size_t)top * width * sizeof(*before)));
    assert(after[top * width + 6] == 0xff8080ffu);

    const char* names[40];
    for(unsigned index = 0; index < 40; ++index) {
        names[index] = index == 39 ? "B" : "A";
    }

    log_text_t saved = text;
    log_text_draw_menu(&text, names, 40, 0, after, width, height, width);
    assert(!memcmp(before, after, (size_t)top * width * sizeof(*before)));
    assert(after[(top + 16) * width + 7] == 0xffffd080u);
    assert(after[(top + 16) * width + 18] == 0xff18120cu);
    assert(after[(top + 16) * width + 19] == 0xffffd080u);

    /* 25 entries fit with this header: item 39 is row 14 of page two. */
    log_text_draw_menu(&text, names, 40, 39, after, width, height, width);
    unsigned selected_y = top + (14 + 2) * 8;
    assert(after[selected_y * width + 7] == 0xffffd080u);
    assert(after[selected_y * width + 18] == 0xffffd080u);
    assert(!memcmp(&saved, &text, sizeof(text)));
    assert(!memcmp(before, after, (size_t)top * width * sizeof(*before)));

    free(after);
    free(before);
}

int main(void)
{
    __wrap();
    __scroll();
    __fragments();
    __pixels(480, 272, 512);
    __pixels(960, 544, 960);
    __pixels(40, 40, 64);
    __pixels(32, 32, 64);
    __fixed_header_and_menu();
    puts("Log text: wrapping, scrolling, fixed header, paged selection and framebuffer bounds passed.");
    return 0;
}
