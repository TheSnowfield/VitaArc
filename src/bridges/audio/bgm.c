#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <logcat/logcat.h>
#include "state.h"

bool vita_audio_load_bgm(audio_t* state, const char* path, int channel)
{
    if(!state || !path || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return false;
    }

    char resolved[512];
    if(!audio_path(path, resolved, sizeof(resolved))) {
        return false;
    }

    audio_stream_t* stream = vita_audio_stream_open(resolved, state->has_mp3);
    if(!stream) {
        log_e(TAG, "Unable to load BGM: %s", resolved);
        return false;
    }

    pthread_mutex_lock(&state->mutex);
    audio_track_t* bgm = &state->bgm[channel];
    audio_stream_t* previous = bgm->stream;
    memset(bgm, 0, sizeof(*bgm));
    bgm->stream = stream;
    bgm->volume = 1.0f;
    snprintf(bgm->path, sizeof(bgm->path), "%s", resolved);
    pthread_mutex_unlock(&state->mutex);
    vita_audio_stream_close(previous);
    log_i(TAG, "Loaded BGM channel %d: %s", channel, resolved);
    return true;
}

void vita_audio_unload_bgm(audio_t* state, int channel)
{
    if(!state || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    audio_track_t* bgm = &state->bgm[channel];
    vita_audio_stream_close(bgm->stream);
    memset(bgm, 0, sizeof(*bgm));
    pthread_mutex_unlock(&state->mutex);
}

void vita_audio_play_bgm(audio_t* state, bool loop, int channel, int delay_ms)
{
    if(!state || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    audio_track_t* bgm = &state->bgm[channel];
    if(bgm->stream && vita_audio_stream_reset(bgm->stream)) {
        bgm->loop = loop;
        bgm->paused = false;
        bgm->playing = true;
        bgm->delay = delay_ms > 0 ? (((uint64_t)delay_ms * VITA_AUDIO_SAMPLE_RATE) / 1000) : 0;
    }

    pthread_mutex_unlock(&state->mutex);
}

int vita_audio_get_bgm_position(audio_t* state, int channel)
{
    if(!state || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return -1;
    }

    pthread_mutex_lock(&state->mutex);
    audio_stream_t* stream = state->bgm[channel].stream;
    uint64_t position = vita_audio_stream_position_frames(stream) / (VITA_AUDIO_SAMPLE_RATE / 1000);
    int result = stream ? (position > INT_MAX ? INT_MAX : (int)position) : -1;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

bool vita_audio_seek_bgm(audio_t* state, int position_ms, int channel)
{
    if(!state || position_ms < 0 || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return false;
    }

    pthread_mutex_lock(&state->mutex);
    audio_track_t* bgm = &state->bgm[channel];
    uint64_t position = (uint64_t)position_ms * (VITA_AUDIO_SAMPLE_RATE / 1000);
    bool result = vita_audio_stream_seek_frames(bgm->stream, position);
    if(result) {
        bgm->delay = 0;
    }

    pthread_mutex_unlock(&state->mutex);
    return result;
}

void vita_audio_set_channel_bgm_volume(audio_t* state, float volume, int channel)
{
    if(!state || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    state->bgm[channel].volume = audio_gain(volume);
    pthread_mutex_unlock(&state->mutex);
}

void vita_audio_set_bgm_paused(audio_t* state, bool paused, int channel)
{
    if(!state || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    state->bgm[channel].paused = paused;
    pthread_mutex_unlock(&state->mutex);
}

void vita_audio_set_bgm_volume(audio_t* state, float volume)
{
    if(!state) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    state->bgm_volume = audio_gain(volume);
    pthread_mutex_unlock(&state->mutex);
}
