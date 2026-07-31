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

int vita_audio_play_sfx_ex(audio_t* state, const char* path, int loop_count, bool force, float volume)
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
    voice->asset = asset;
    if(asset)
        asset->last_handle = handle;
    pthread_mutex_unlock(&state->mutex);
    return asset ? handle : -1;
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
