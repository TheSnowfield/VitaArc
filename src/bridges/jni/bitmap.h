#ifndef VITAARC_JNI_BITMAP_H
#define VITAARC_JNI_BITMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "aosp/jni.h"

typedef struct bridge_bitmap_request {
    const uint8_t* text;
    size_t text_size;
    const char* font_path;
    int font_size, width, height, alignment, overflow;
    bool wrap, stroke;
    float stroke_size;
    uint8_t color[4], stroke_color[4];
} bridge_bitmap_request_t;

typedef struct bridge_bitmap {
    int width, height;
    uint8_t* rgba; /* Top-down, premultiplied RGBA, matching Android's bitmap. */
} bridge_bitmap_t;

bool bridge_bitmap_render(const bridge_bitmap_request_t* request, bridge_bitmap_t* output);
bool jni_bitmap_create(JNIEnv* env, const jvalue* arguments);

#endif
