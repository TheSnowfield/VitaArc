#include <pthread.h>
#include <string.h>

#include "mp3_marks.h"

#define MARK_COUNT 16u

typedef struct mp3_mark_row_t {
    char path[MP3_MARK_PATH];
    long first;
    mp3_frame_t format;
    mp3_mark_t mark;
} mp3_mark_row_t;

static mp3_mark_row_t __rows[MARK_COUNT];
static uint32_t __next;
static pthread_mutex_t __mutex = PTHREAD_MUTEX_INITIALIZER;

bool mp3_mark_get(const char* path, long first, const mp3_frame_t* format, mp3_mark_t* mark)
{
    if(!path || !path[0]) {
        return false;
    }

    pthread_mutex_lock(&__mutex);
    for(uint32_t index = 0; index < MARK_COUNT; ++index) {
        const mp3_mark_row_t* row = &__rows[index];
        if(row->first != first || row->mark.target != mark->target) {
            continue;
        }

        if(strcmp(row->path, path) || !mp3_frame_matches(&row->format, format)) {
            continue;
        }

        *mark = row->mark;
        pthread_mutex_unlock(&__mutex);
        return true;
    }

    pthread_mutex_unlock(&__mutex);
    return false;
}

void mp3_mark_put(const char* path, long first, const mp3_frame_t* format, const mp3_mark_t* mark)
{
    if(!path || !path[0] || strlen(path) >= MP3_MARK_PATH) {
        return;
    }

    pthread_mutex_lock(&__mutex);
    mp3_mark_row_t* row = &__rows[__next];
    strcpy(row->path, path);
    row->first = first;
    row->format = *format;
    row->mark = *mark;
    __next = (__next + 1) % MARK_COUNT;
    pthread_mutex_unlock(&__mutex);
}

void mp3_mark_clear(void)
{
    pthread_mutex_lock(&__mutex);
    memset(__rows, 0, sizeof(__rows));
    __next = 0;
    pthread_mutex_unlock(&__mutex);
}
