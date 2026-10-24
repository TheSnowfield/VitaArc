#ifndef VITAARC_MP3_MARKS_H
#define VITAARC_MP3_MARKS_H

#include "mp3_frame.h"

#define MP3_MARK_PATH 256u

typedef struct mp3_mark_t {
    uint32_t target;
    uint32_t frame;
    long offset;
} mp3_mark_t;

bool mp3_mark_get(const char* path, long first, const mp3_frame_t* format, mp3_mark_t* mark);
void mp3_mark_put(const char* path, long first, const mp3_frame_t* format, const mp3_mark_t* mark);
void mp3_mark_clear(void);

#endif
