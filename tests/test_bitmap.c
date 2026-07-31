#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jni/bitmap.h>

static unsigned pixels(const bridge_bitmap_t* bitmap, int* minimum_x, int* minimum_y)
{
    unsigned count = 0;
    *minimum_x = bitmap->width;
    *minimum_y = bitmap->height;
    for(int y = 0; y < bitmap->height; ++y)
        for(int x = 0; x < bitmap->width; ++x) {
            const uint8_t* pixel = bitmap->rgba + ((size_t)y * bitmap->width + x) * 4;
            assert(pixel[0] <= pixel[3] && pixel[1] <= pixel[3] && pixel[2] <= pixel[3]);
            if(pixel[3]) {
                ++count;
                if(x < *minimum_x)
                    *minimum_x = x;
                if(y < *minimum_y)
                    *minimum_y = y;
            }
        }

    return count;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    bridge_bitmap_request_t request = {.text = (const uint8_t*)"Arcaea",
        .text_size = 6,
        .font_path = argv[1],
        .font_size = 28,
        .width = 300,
        .height = 100,
        .alignment = 0x11,
        .color = {200, 100, 50, 128},
        .stroke_color = {10, 20, 30, 255}};

    bridge_bitmap_t bitmap;
    int left, top, right, bottom, dummy;
    assert(bridge_bitmap_render(&request, &bitmap));
    unsigned basic = pixels(&bitmap, &left, &top);
    assert(basic > 200 && bitmap.width == 300 && bitmap.height == 100);
    free(bitmap.rgba);
    request.alignment = 0x22;
    assert(bridge_bitmap_render(&request, &bitmap));
    assert(pixels(&bitmap, &right, &bottom) == basic && right > left + 100 && bottom > top + 30);
    free(bitmap.rgba);
    request.alignment = 0x33;
    assert(bridge_bitmap_render(&request, &bitmap));
    assert(pixels(&bitmap, &dummy, &top) == basic && dummy > left && dummy < right);
    free(bitmap.rgba);
    request.stroke = true;
    request.stroke_size = 3;
    assert(bridge_bitmap_render(&request, &bitmap));
    assert(pixels(&bitmap, &dummy, &top) > basic);
    free(bitmap.rgba);
    request.stroke = false;
    request.width = 0;
    request.height = 0;
    request.alignment = 0x11;
    request.text = (const uint8_t*)"one\ntwo";
    request.text_size = 7;
    assert(bridge_bitmap_render(&request, &bitmap));
    int multiline_height = bitmap.height;
    assert(multiline_height > 40 && pixels(&bitmap, &dummy, &top) > 100);
    free(bitmap.rgba);
    request.text = (const uint8_t*)"one two three four";
    request.text_size = 18;
    request.width = 85;
    request.wrap = true;
    assert(bridge_bitmap_render(&request, &bitmap));
    assert(bitmap.height >= multiline_height && bitmap.width == 85);
    free(bitmap.rgba);
    request.width = 70;
    request.height = 25;
    request.wrap = false;
    request.overflow = 2;
    assert(bridge_bitmap_render(&request, &bitmap));
    assert(pixels(&bitmap, &dummy, &top) > 20 && bitmap.width == 70 && bitmap.height == 25);
    free(bitmap.rgba);
    request.text = (const uint8_t*)"\xc3\xa9\xce\xa9";
    request.text_size = 4;
    request.width = 0;
    request.height = 0;
    request.overflow = 0;
    assert(bridge_bitmap_render(&request, &bitmap));
    assert(pixels(&bitmap, &dummy, &top) > 50);
    free(bitmap.rgba);
    request.text_size = 0;
    assert(!bridge_bitmap_render(&request, &bitmap) && !bitmap.rgba);
    request.text_size = 4;
    request.width = INT32_MAX;
    assert(!bridge_bitmap_render(&request, &bitmap) && !bitmap.rgba);
    request.width = 0;
    request.font_size = 0;
    assert(!bridge_bitmap_render(&request, &bitmap));
    request.font_size = 28;
    request.stroke = true;
    request.stroke_size = NAN;
    assert(!bridge_bitmap_render(&request, &bitmap));
    request.stroke = false;
    request.font_path = "missing-font.ttf";
    assert(!bridge_bitmap_render(&request, &bitmap) && !bitmap.rgba);
    assert(!bridge_bitmap_render(NULL, &bitmap) && !bitmap.rgba);
    puts(
        "Text bitmaps pass: premultiplied pixels, alignment, outline, newline/wrap/shrink, Unicode, invalid bounds/font and cleanup.");
}
