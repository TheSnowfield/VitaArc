#ifndef VITAARC_AUDIO_STATE_H
#define VITAARC_AUDIO_STATE_H

#include <pthread.h>
#include <stdatomic.h>
#include <psp2/types.h>
#include "backend.h"
#include "stream.h"
#include "wav.h"

/* The backend owns this state. Playback and mixing share the same mutex;
 * decoding resources are released only after the output thread has stopped. */
#define VITA_AUDIO_GRAIN VITA_AUDIO_BLOCK_FRAMES
#define VITA_AUDIO_CHANNELS 2
#define VITA_AUDIO_MAX_SFX 32
#define VITA_AUDIO_MAX_VOICES 32

typedef struct audio_asset_t {
    bool used;
    char* path;
    audio_pcm_t pcm;
    int last_handle;
} audio_asset_t;

typedef struct audio_voice_t {
    bool active;
    float volume;
    int loops;
    int handle;
    uint32_t position;
    audio_asset_t* asset;
} audio_voice_t;

typedef struct audio_track_t {
    audio_stream_t* stream;
    bool playing;
    bool paused;
    bool loop;
    uint64_t delay;
    float volume;
    char path[512];
} audio_track_t;

struct audio_t {
    /* Output worker. */
    pthread_mutex_t mutex;
    SceUID thread;
    int port;
    atomic_bool running;
    bool started;
    bool suspended;
    bool has_mp3;

    /* Playback controls. */
    float sfx_volume;
    float bgm_volume;
    int serial;

    /* Cached samples, active voices and streaming tracks. */
    audio_asset_t sfx[VITA_AUDIO_MAX_SFX];
    audio_voice_t voices[VITA_AUDIO_MAX_VOICES];
    audio_track_t bgm[VITA_AUDIO_BGM_CHANNELS];
};

float audio_gain(float volume);
bool audio_path(const char* source, char* destination, size_t destination_size);

#endif
