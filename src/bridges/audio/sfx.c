#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <logcat/logcat.h>
#include "state.h"

static audio_asset_t* find_sfx(audio_t* state, const char* path)
{
    for(uint32_t i = 0; i < VITA_AUDIO_MAX_SFX; ++i) {
        if(state->sfx[i].used && strcmp(state->sfx[i].path, path) == 0) {
            return &state->sfx[i];
        }
    }

    return NULL;
}

static audio_asset_t* find_free_sfx(audio_t* state)
{
    for(uint32_t i = 0; i < VITA_AUDIO_MAX_SFX; ++i) {
        if(!state->sfx[i].used) {
            return &state->sfx[i];
        }
    }

    return NULL;
}

bool vita_audio_preload_sfx(audio_t* state, const char* path)
{
    if(!state || !path) {
        return false;
    }

    char resolved[512];
    if(!audio_path(path, resolved, sizeof(resolved))) {
        return false;
    }

    pthread_mutex_lock(&state->mutex);
    if(find_sfx(state, resolved)) {
        pthread_mutex_unlock(&state->mutex);
        return true;
    }

    audio_asset_t* asset = find_free_sfx(state);
    pthread_mutex_unlock(&state->mutex);
    if(!asset) {
        log_e(TAG, "SFX cache is full: %s", resolved);
        return false;
    }

    audio_pcm_t pcm;
    if(!vita_wav_load(resolved, VITA_AUDIO_SAMPLE_RATE, &pcm)) {
        log_e(TAG, "Unable to load SFX: %s", resolved);
        return false;
    }

    char* stored_path = strdup(resolved);
    if(!stored_path) {
        vita_wav_free(&pcm);
        return false;
    }

    pthread_mutex_lock(&state->mutex);
    audio_asset_t* existing = find_sfx(state, resolved);
    if(existing) {
        pthread_mutex_unlock(&state->mutex);
        free(stored_path);
        vita_wav_free(&pcm);
        return true;
    }

    asset = find_free_sfx(state);
    if(!asset) {
        pthread_mutex_unlock(&state->mutex);
        free(stored_path);
        vita_wav_free(&pcm);
        return false;
    }

    asset->used = true;
    asset->path = stored_path;
    asset->pcm = pcm;
    pthread_mutex_unlock(&state->mutex);

    log_i(TAG, "Preloaded SFX: %s", resolved);
    return true;
}

void vita_audio_unload_sfx(audio_t* state, const char* path)
{
    if(!state || !path) {
        return;
    }

    char resolved[512];
    if(!audio_path(path, resolved, sizeof(resolved))) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    audio_asset_t* asset = find_sfx(state, resolved);
    if(asset) {
        for(size_t index = 0; index < state->event_count; ++index) {
            if(state->events[index].asset == asset) {
                state->events[index].played = true;
            }
        }

        for(uint32_t i = 0; i < VITA_AUDIO_MAX_VOICES; ++i) {
            if(state->voices[i].asset == asset) {
                memset(&state->voices[i], 0, sizeof(state->voices[i]));
            }
        }

        vita_wav_free(&asset->pcm);
        free(asset->path);
        memset(asset, 0, sizeof(*asset));
    }

    pthread_mutex_unlock(&state->mutex);
}

int vita_audio_play_sfx(audio_t* state, const char* path, int unused, bool force)
{
    return vita_audio_play_sfx_ex(state, path, unused, force, -1.0f);
}

audio_voice_t* audio_sfx_start(audio_t* state, audio_asset_t* asset, int loop_count, bool force, float volume)
{
    audio_voice_t* voice = NULL;
    for(uint32_t i = 0; i < VITA_AUDIO_MAX_VOICES; ++i) {
        if(!state->voices[i].active) {
            voice = &state->voices[i];
            break;
        }
    }

    if(!voice) {
        voice = &state->voices[0];
    }

    int handle = state->serial;
    if(state->serial == INT_MAX) {
        state->serial = 1;
    } else {
        ++state->serial;
    }

    voice->active = asset != NULL;
    voice->volume = force ? 0.8f : volume == -1.0f ? -1.0f : audio_gain(volume);
    voice->loops = loop_count < 0 ? -1 : loop_count;
    voice->handle = handle;
    voice->position = 0;
    voice->begin = 0;
    voice->end = asset ? asset->pcm.frame_count : 0;
    voice->delay = 0;
    voice->asset = asset;
    if(asset) {
        asset->last_handle = handle;
    }

    return voice;
}

static int __play(audio_t* state, const char* path, int loop_count, bool force, float volume, int begin, int end)
{
    if(!state || !path) {
        return -1;
    }

    char resolved[512];
    if(!audio_path(path, resolved, sizeof(resolved))) {
        return -1;
    }

    if(!vita_audio_preload_sfx(state, path)) {
        return -1;
    }

    pthread_mutex_lock(&state->mutex);
    audio_asset_t* asset = find_sfx(state, resolved);
    audio_voice_t* voice = audio_sfx_start(state, asset, loop_count, force, volume);
    if(asset && begin >= 0 && end > begin) {
        uint64_t first = (uint64_t)begin * VITA_AUDIO_SAMPLE_RATE / 1000;
        uint64_t last = (uint64_t)end * VITA_AUDIO_SAMPLE_RATE / 1000;
        if(last <= asset->pcm.frame_count) {
            voice->begin = first;
            voice->end = last;
        }
    }

    int handle = asset ? voice->handle : -1;
    pthread_mutex_unlock(&state->mutex);
    return handle;
}

int vita_audio_play_sfx_ex(audio_t* state, const char* path, int loop_count, bool force, float volume)
{
    return __play(state, path, loop_count, force, volume, 0, 0);
}

int vita_audio_play_sfx_range(audio_t* state, const char* path, bool loop, bool force, float volume, int begin, int end)
{
    return __play(state, path, loop ? -1 : 0, force, volume, begin, end);
}

bool vita_audio_schedule_sfx(audio_t* state, const char* path, int position)
{
    if(!state || position < 0) {
        return false;
    }

    char resolved[512];
    if(!audio_path(path, resolved, sizeof(resolved)) || !vita_audio_preload_sfx(state, path)) {
        return false;
    }

    pthread_mutex_lock(&state->mutex);
    audio_asset_t* asset = find_sfx(state, resolved);
    if(!asset) {
        pthread_mutex_unlock(&state->mutex);
        return false;
    }

    if(state->event_count == state->event_capacity) {
        size_t capacity = state->event_capacity ? state->event_capacity * 2 : 16;
        if(capacity > 4096) {
            pthread_mutex_unlock(&state->mutex);
            return false;
        }

        audio_event_t* events = realloc(state->events, capacity * sizeof(*events));
        if(!events) {
            pthread_mutex_unlock(&state->mutex);
            return false;
        }

        state->events = events;
        state->event_capacity = capacity;
    }

    uint64_t frame = (uint64_t)position * VITA_AUDIO_SAMPLE_RATE / 1000;
    size_t index = state->event_count;
    while(index && state->events[index - 1].frame > frame) {
        state->events[index] = state->events[index - 1];
        --index;
    }

    if(index < state->event_next) {
        state->event_next = index;
    }

    ++state->event_count;
    audio_event_t* event = &state->events[index];
    event->asset = asset;
    event->frame = frame;
    event->played = false;
    pthread_mutex_unlock(&state->mutex);
    return true;
}

float vita_audio_get_sfx_volume(audio_t* state)
{
    if(!state) {
        return 0.0f;
    }

    pthread_mutex_lock(&state->mutex);
    float volume = state->sfx_volume;
    pthread_mutex_unlock(&state->mutex);
    return volume;
}

bool vita_audio_is_active(audio_t* state)
{
    if(!state) {
        return true;
    }

    pthread_mutex_lock(&state->mutex);
    bool active = !state->suspended;
    pthread_mutex_unlock(&state->mutex);
    return active;
}

/* The newer FMOD provider remembers the most recently played channel for
 * each filename. Operations on one file must not alter other voices. */
static audio_voice_t* find_sfx_channel(audio_t* state, const char* path)
{
    char resolved[512];
    if(!audio_path(path, resolved, sizeof(resolved)))
        return NULL;

    audio_asset_t* asset = find_sfx(state, resolved);
    if(!asset)
        return NULL;

    for(unsigned index = 0; index < VITA_AUDIO_MAX_VOICES; ++index) {
        audio_voice_t* voice = &state->voices[index];
        if(voice->asset == asset && voice->handle == asset->last_handle)
            return voice;
    }

    return NULL;
}

void vita_audio_stop_sfx_path(audio_t* state, const char* path)
{
    if(!state || !path)
        return;

    pthread_mutex_lock(&state->mutex);
    audio_voice_t* voice = find_sfx_channel(state, path);
    if(voice)
        voice->active = false;
    pthread_mutex_unlock(&state->mutex);
}

float vita_audio_get_sfx_path_volume(audio_t* state, const char* path)
{
    if(!state || !path)
        return 0.0f;

    pthread_mutex_lock(&state->mutex);
    audio_voice_t* voice = find_sfx_channel(state, path);
    float result = voice ? (voice->volume < 0 ? state->sfx_volume : voice->volume) : 0.0f;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

void vita_audio_set_sfx_path_volume(audio_t* state, const char* path, float volume)
{
    if(!state || !path)
        return;

    pthread_mutex_lock(&state->mutex);
    audio_voice_t* voice = find_sfx_channel(state, path);
    if(voice)
        voice->volume = audio_gain(volume);
    pthread_mutex_unlock(&state->mutex);
}

void vita_audio_stop_sfx(audio_t* state, int handle)
{
    if(!state || handle < 0) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    for(uint32_t i = 0; i < VITA_AUDIO_MAX_VOICES; ++i) {
        if(state->voices[i].active && state->voices[i].handle == handle) {
            memset(&state->voices[i], 0, sizeof(state->voices[i]));
            break;
        }
    }

    pthread_mutex_unlock(&state->mutex);
}

void vita_audio_set_sfx_volume(audio_t* state, float volume)
{
    if(!state) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    state->sfx_volume = audio_gain(volume);
    pthread_mutex_unlock(&state->mutex);
}
