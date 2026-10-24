#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "backend.h"
#include "feed.h"
#include "stream.h"

#define FEED_FRAMES VITA_AUDIO_BLOCK_FRAMES
#define FEED_BLOCKS 16u
#define FEED_PRIME 4u
#define FEED_STACK (128 * 1024)

typedef struct audio_block_t {
    uint64_t start;
    uint64_t end;
    uint32_t frames;
    bool ended;
    int16_t pcm[FEED_FRAMES * 2];
} audio_block_t;

struct audio_feed_t {
    /* The worker exclusively owns the codec and its file. */
    audio_stream_t* stream;
    pthread_t worker;
    atomic_uint refs;
    bool started;

    /* Commands are serialized separately from short PCM queue operations. */
    pthread_mutex_t control;
    pthread_mutex_t mutex;
    pthread_cond_t work;
    pthread_cond_t ready;
    bool running;
    bool command;
    bool seeking;
    bool result;
    bool loop;
    bool next_loop;
    uint64_t target;

    /* Positions follow consumed samples, never the decoder's read-ahead. */
    audio_block_t blocks[FEED_BLOCKS];
    uint32_t head;
    uint32_t count;
    uint32_t offset;
    uint64_t position;
    bool eof;
};

static bool __sync_init(audio_feed_t* feed)
{
    if(pthread_mutex_init(&feed->mutex, NULL) != 0) {
        return false;
    }

    if(pthread_mutex_init(&feed->control, NULL) != 0) {
        pthread_mutex_destroy(&feed->mutex);
        return false;
    }

    if(pthread_cond_init(&feed->work, NULL) != 0) {
        pthread_mutex_destroy(&feed->control);
        pthread_mutex_destroy(&feed->mutex);
        return false;
    }

    if(pthread_cond_init(&feed->ready, NULL) != 0) {
        pthread_cond_destroy(&feed->work);
        pthread_mutex_destroy(&feed->control);
        pthread_mutex_destroy(&feed->mutex);
        return false;
    }

    return true;
}

static void __decode(audio_feed_t* feed, audio_block_t* block, bool loop)
{
    block->frames = 0;
    block->start = vita_audio_stream_position_frames(feed->stream);
    block->end = block->start;
    block->ended = false;

    for(uint32_t frame = 0; frame < FEED_FRAMES; ++frame) {
        int16_t* pcm = &block->pcm[frame * 2];
        if(!vita_audio_stream_next_sample(feed->stream, loop, pcm, pcm + 1)) {
            block->ended = true;
            break;
        }

        ++block->frames;
        uint64_t position = vita_audio_stream_position_frames(feed->stream);
        bool wrapped = position < block->end;
        block->end = position;
        if(wrapped) {
            break;
        }
    }
}

static bool __fill(audio_feed_t* feed, bool prime)
{
    pthread_mutex_lock(&feed->mutex);
    if(!feed->running || feed->eof || feed->count == FEED_BLOCKS) {
        pthread_mutex_unlock(&feed->mutex);
        return false;
    }

    bool loop = feed->loop;
    pthread_mutex_unlock(&feed->mutex);

    audio_block_t block = {0};
    __decode(feed, &block, loop);

    pthread_mutex_lock(&feed->mutex);
    if(!feed->running || (feed->seeking && !prime)) {
        pthread_mutex_unlock(&feed->mutex);
        return false;
    }

    if(block.frames) {
        uint32_t tail = (feed->head + feed->count) % FEED_BLOCKS;
        feed->blocks[tail] = block;
        ++feed->count;
    }

    feed->eof = block.ended;
    pthread_mutex_unlock(&feed->mutex);
    return block.frames != 0;
}

static void __seek(audio_feed_t* feed)
{
    pthread_mutex_lock(&feed->mutex);
    uint64_t target = feed->target;
    uint64_t previous = feed->position;
    bool loop = feed->next_loop;
    bool old_loop = feed->loop;
    feed->command = false;
    pthread_mutex_unlock(&feed->mutex);

    bool result = vita_audio_stream_seek_frames(feed->stream, target);
    bool restored = result;
    if(!result) {
        restored = vita_audio_stream_seek_frames(feed->stream, previous);
    }

    pthread_mutex_lock(&feed->mutex);
    feed->head = 0;
    feed->count = 0;
    feed->offset = 0;
    feed->eof = !restored;
    feed->loop = result ? loop : old_loop;
    feed->position = vita_audio_stream_position_frames(feed->stream);
    pthread_mutex_unlock(&feed->mutex);

    for(uint32_t block = 0; block < FEED_PRIME; ++block) {
        if(!__fill(feed, true)) {
            break;
        }
    }

    pthread_mutex_lock(&feed->mutex);
    feed->result = result;
    feed->seeking = false;
    pthread_cond_broadcast(&feed->ready);
    pthread_mutex_unlock(&feed->mutex);
}

static void* __worker(void* argument)
{
    audio_feed_t* feed = argument;

    pthread_mutex_lock(&feed->mutex);
    while(feed->running) {
        if(feed->command) {
            pthread_mutex_unlock(&feed->mutex);
            __seek(feed);
            pthread_mutex_lock(&feed->mutex);
            continue;
        }

        if(feed->eof || feed->count == FEED_BLOCKS) {
            pthread_cond_wait(&feed->work, &feed->mutex);
            continue;
        }

        pthread_mutex_unlock(&feed->mutex);
        __fill(feed, false);
        pthread_mutex_lock(&feed->mutex);
    }

    pthread_mutex_unlock(&feed->mutex);
    return NULL;
}

audio_feed_t* audio_feed_open(const char* path, bool has_mp3)
{
    audio_stream_t* stream = vita_audio_stream_open(path, has_mp3);
    if(!stream) {
        return NULL;
    }

    audio_feed_t* feed = calloc(1, sizeof(*feed));
    if(!feed) {
        vita_audio_stream_close(stream);
        return NULL;
    }

    if(!__sync_init(feed)) {
        vita_audio_stream_close(stream);
        free(feed);
        return NULL;
    }

    feed->stream = stream;
    feed->running = true;
    atomic_init(&feed->refs, 1);

    pthread_attr_t attr;
    if(pthread_attr_init(&attr) != 0) {
        audio_feed_close(feed);
        return NULL;
    }

    int result = pthread_attr_setstacksize(&attr, FEED_STACK);
    if(result == 0) {
        result = pthread_create(&feed->worker, &attr, __worker, feed);
    }

    pthread_attr_destroy(&attr);
    if(result != 0) {
        audio_feed_close(feed);
        return NULL;
    }

    feed->started = true;
    if(!audio_feed_seek(feed, 0, false)) {
        audio_feed_close(feed);
        return NULL;
    }

    return feed;
}

audio_feed_t* audio_feed_retain(audio_feed_t* feed)
{
    if(feed) {
        atomic_fetch_add(&feed->refs, 1);
    }

    return feed;
}

void audio_feed_close(audio_feed_t* feed)
{
    if(!feed || atomic_fetch_sub(&feed->refs, 1) != 1) {
        return;
    }

    pthread_mutex_lock(&feed->mutex);
    feed->running = false;
    pthread_cond_broadcast(&feed->work);
    pthread_cond_broadcast(&feed->ready);
    pthread_mutex_unlock(&feed->mutex);

    if(feed->started) {
        pthread_join(feed->worker, NULL);
    }

    vita_audio_stream_close(feed->stream);
    pthread_cond_destroy(&feed->ready);
    pthread_cond_destroy(&feed->work);
    pthread_mutex_destroy(&feed->control);
    pthread_mutex_destroy(&feed->mutex);
    free(feed);
}

bool audio_feed_seek(audio_feed_t* feed, uint64_t position, bool loop)
{
    if(!feed) {
        return false;
    }

    pthread_mutex_lock(&feed->control);
    pthread_mutex_lock(&feed->mutex);
    feed->target = position;
    feed->next_loop = loop;
    feed->seeking = true;
    feed->command = true;
    pthread_cond_signal(&feed->work);

    while(feed->running && feed->seeking) {
        pthread_cond_wait(&feed->ready, &feed->mutex);
    }

    bool result = feed->running && feed->result;
    pthread_mutex_unlock(&feed->mutex);
    pthread_mutex_unlock(&feed->control);
    return result;
}

uint64_t audio_feed_position(audio_feed_t* feed)
{
    if(!feed) {
        return 0;
    }

    pthread_mutex_lock(&feed->mutex);
    uint64_t position = feed->position;
    pthread_mutex_unlock(&feed->mutex);
    return position;
}

uint32_t audio_feed_read(audio_feed_t* feed, int16_t* pcm, uint32_t frames, bool* ended)
{
    if(!ended) {
        return 0;
    }

    *ended = false;
    if(!feed || !pcm || !frames) {
        return 0;
    }

    pthread_mutex_lock(&feed->mutex);
    if(feed->seeking) {
        pthread_mutex_unlock(&feed->mutex);
        return 0;
    }

    uint32_t copied = 0;
    while(copied < frames && feed->count) {
        audio_block_t* block = &feed->blocks[feed->head];
        uint32_t count = block->frames - feed->offset;
        if(count > frames - copied) {
            count = frames - copied;
        }

        memcpy(pcm + copied * 2, block->pcm + feed->offset * 2, count * 2 * sizeof(*pcm));
        copied += count;
        feed->offset += count;
        feed->position = block->start + feed->offset;
        if(feed->offset == block->frames) {
            feed->position = block->end;
            feed->head = (feed->head + 1) % FEED_BLOCKS;
            feed->offset = 0;
            --feed->count;
        }
    }

    *ended = feed->eof && !feed->count;
    if(copied) {
        pthread_cond_signal(&feed->work);
    }

    pthread_mutex_unlock(&feed->mutex);
    return copied;
}
