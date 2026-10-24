#ifndef VITAARC_MP3_INDEX_H
#define VITAARC_MP3_INDEX_H

#include "mp3_frame.h"
#include "mp3_marks.h"

#define MP3_SEEK_PREROLL 16u

typedef struct mp3_index_t {
    char path[MP3_MARK_PATH];
    long* offsets;
    uint32_t count;
    uint32_t capacity;
    uint32_t scanned;
    long first;
    long next;
    bool blocked;
} mp3_index_t;

void mp3_index_init(mp3_index_t* index, long first, const char* path);
void mp3_index_free(mp3_index_t* index);

/**
 * Find an exact warm-up boundary using sparse checkpoints and frame payloads.
 * Reopened app0 assets may reuse a bounded process-local preview mark cache.
 * Scans restore the decoder's file position and never decode skipped frames.
 */
bool mp3_index_find(mp3_index_t* index, FILE* file, const mp3_frame_t* format,
    uint32_t target, long* offset, uint32_t* frame);

#endif
