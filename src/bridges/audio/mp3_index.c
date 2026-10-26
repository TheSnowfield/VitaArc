#include <limits.h>
#include <psp2/audiodec.h>
#include <stdlib.h>
#include <string.h>
#include <config.h>

#include "mp3_index.h"

#define INDEX_STRIDE 32u
#define INDEX_HISTORY 4u

void mp3_index_init(mp3_index_t* index, long first, const char* path)
{
    *index = (mp3_index_t){.first = first, .next = first};
    if(!path || (strncmp(path, "app0:", 5) && strncmp(path, PATH_TO_PACKAGE, strlen(PATH_TO_PACKAGE))) ||
        strlen(path) >= sizeof(index->path)) {
        return;
    }

    /* The selected version stays fixed while the game is running. */
    strcpy(index->path, path);
}

void mp3_index_free(mp3_index_t* index)
{
    free(index->offsets);
    *index = (mp3_index_t){0};
}

static bool __append(mp3_index_t* index)
{
    if(index->count == index->capacity) {
        if(index->capacity > UINT32_MAX / sizeof(*index->offsets) - 64) {
            return false;
        }

        uint32_t capacity = index->capacity + 64;
        long* offsets = realloc(index->offsets, capacity * sizeof(*offsets));
        if(!offsets) {
            return false;
        }

        index->offsets = offsets;
        index->capacity = capacity;
    }

    index->offsets[index->count++] = index->next;
    return true;
}

static bool __read(FILE* file, const mp3_frame_t* format, mp3_frame_t* frame)
{
    uint8_t data[SCE_AUDIODEC_MP3_MAX_ES_SIZE];
    if(fread(data, 1, 4, file) != 4 || !mp3_frame_parse(data, frame)) {
        return false;
    }

    if(!mp3_frame_matches(frame, format)) {
        return false;
    }

    return fread(data + 4, 1, frame->size - 4, file) == frame->size - 4;
}

static bool __scan(mp3_index_t* index, FILE* file, const mp3_frame_t* format, uint32_t target)
{
    if(fseek(file, index->next, SEEK_SET) != 0) {
        return false;
    }

    while(index->scanned <= target) {
        mp3_frame_t frame;
        if(!__read(file, format, &frame)) {
            return false;
        }

        if(index->next > LONG_MAX - (long)frame.size) {
            return false;
        }

        if(index->scanned % INDEX_STRIDE == 0 && !__append(index)) {
            return false;
        }

        index->next += frame.size;
        ++index->scanned;
    }

    return true;
}

static bool __locate(mp3_index_t* index, FILE* file, const mp3_frame_t* format, mp3_mark_t* mark)
{
    uint32_t start = mark->target - MP3_SEEK_PREROLL;
    uint32_t slot = start / INDEX_STRIDE;
    long offset = index->offsets[slot];
    if(fseek(file, offset, SEEK_SET) != 0) {
        return false;
    }

    long offsets[MP3_SEEK_PREROLL];
    uint32_t payloads[MP3_SEEK_PREROLL];
    for(uint32_t frame = slot * INDEX_STRIDE; frame < mark->target; ++frame) {
        mp3_frame_t packet;
        if(!__read(file, format, &packet) || offset > LONG_MAX - (long)packet.size) {
            return false;
        }

        if(frame >= start) {
            offsets[frame - start] = offset;
            payloads[frame - start] = packet.payload;
        }

        offset += packet.size;
    }

    /* Keep two full synthesis history frames after filling the bit reservoir. */
    uint32_t reservoir = format->version == SCE_AUDIODEC_MP3_MPEG_VERSION_1 ? 511 : 255;
    uint32_t first = MP3_SEEK_PREROLL - INDEX_HISTORY;
    uint32_t payload = payloads[first] + payloads[first + 1];
    while(payload < reservoir && first) {
        payload += payloads[--first];
    }

    if(payload < reservoir) {
        return false;
    }

    mark->offset = offsets[first];
    mark->frame = start + first;
    return true;
}

bool mp3_index_find(mp3_index_t* index, FILE* file, const mp3_frame_t* format,
    uint32_t target, long* offset, uint32_t* frame)
{
    if(index->blocked || target < MP3_SEEK_PREROLL) {
        return false;
    }

    mp3_mark_t mark = {.target = target};
    if(!mp3_mark_get(index->path, index->first, format, &mark)) {
        long saved = ftell(file);
        if(saved < 0) {
            return false;
        }

        bool result = true;
        if(target >= index->scanned) {
            result = __scan(index, file, format, target);
        }

        if(result) {
            result = __locate(index, file, format, &mark);
        }

        if(fseek(file, saved, SEEK_SET) != 0) {
            result = false;
        }

        if(!result) {
            index->blocked = true;
            return false;
        }

        mp3_mark_put(index->path, index->first, format, &mark);
    }

    *offset = mark.offset;
    *frame = mark.frame;
    return true;
}
