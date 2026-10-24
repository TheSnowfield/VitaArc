#ifndef VITAARC_MP3_FRAME_H
#define VITAARC_MP3_FRAME_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct mp3_frame_t {
    uint32_t size;
    uint32_t rate;
    uint32_t version;
    uint32_t channels;
    uint32_t samples;
    uint32_t payload;
} mp3_frame_t;

bool mp3_frame_parse(const uint8_t data[4], mp3_frame_t* frame);
bool mp3_frame_read(FILE* file, uint8_t header[4], mp3_frame_t* frame);
bool mp3_frame_matches(const mp3_frame_t* frame, const mp3_frame_t* format);

#endif
