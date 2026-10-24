#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <logcat/logcat.h>
#include "state.h"

static audio_feed_t* __retain(audio_t* state, int channel, bool* loop)
{
    pthread_mutex_lock(&state->mutex);
    audio_track_t* bgm = &state->bgm[channel];
    audio_feed_t* feed = audio_feed_retain(bgm->feed);
    if(loop) {
        *loop = bgm->loop;
    }

    pthread_mutex_unlock(&state->mutex);
    return feed;
}

bool vita_audio_load_bgm(audio_t* state, const char* path, int channel)
{
    if(!state || !path || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return false;
    }

    char resolved[512];
    if(!audio_path(path, resolved, sizeof(resolved))) {
        return false;
    }

    audio_feed_t* feed = audio_feed_open(resolved, state->has_mp3);
    if(!feed) {
        log_e(TAG, "Unable to load BGM: %s", resolved);
        return false;
    }

    pthread_mutex_lock(&state->mutex);
    audio_track_t* bgm = &state->bgm[channel];
    audio_feed_t* previous = bgm->feed;
    memset(bgm, 0, sizeof(*bgm));
    bgm->feed = feed;
    bgm->volume = 1.0f;
    snprintf(bgm->path, sizeof(bgm->path), "%s", resolved);
    pthread_mutex_unlock(&state->mutex);
    audio_feed_close(previous);
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
    audio_feed_t* previous = bgm->feed;
    memset(bgm, 0, sizeof(*bgm));
    pthread_mutex_unlock(&state->mutex);
    audio_feed_close(previous);
}

void vita_audio_play_bgm(audio_t* state, bool loop, int channel, int delay_ms)
{
    if(!state || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return;
    }

    audio_feed_t* feed = __retain(state, channel, NULL);
    if(!feed) {
        return;
    }

    bool ready = audio_feed_seek(feed, 0, loop);
    pthread_mutex_lock(&state->mutex);
    audio_track_t* bgm = &state->bgm[channel];
    if(ready && bgm->feed == feed) {
        bgm->loop = loop;
        bgm->paused = false;
        bgm->playing = true;
        bgm->delay = delay_ms > 0 ? (((uint64_t)delay_ms * VITA_AUDIO_SAMPLE_RATE) / 1000) : 0;
    }

    pthread_mutex_unlock(&state->mutex);
    audio_feed_close(feed);
}

int vita_audio_get_bgm_position(audio_t* state, int channel)
{
    if(!state || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return -1;
    }

    pthread_mutex_lock(&state->mutex);
    audio_feed_t* feed = state->bgm[channel].feed;
    uint64_t position = audio_feed_position(feed) / (VITA_AUDIO_SAMPLE_RATE / 1000);
    int result = feed ? (position > INT_MAX ? INT_MAX : (int)position) : -1;
    pthread_mutex_unlock(&state->mutex);
    return result;
}

bool vita_audio_seek_bgm(audio_t* state, int position_ms, int channel)
{
    if(!state || position_ms < 0 || channel < 0 || channel >= VITA_AUDIO_BGM_CHANNELS) {
        return false;
    }

    bool loop = false;
    audio_feed_t* feed = __retain(state, channel, &loop);
    if(!feed) {
        return false;
    }

    uint64_t position = (uint64_t)position_ms * (VITA_AUDIO_SAMPLE_RATE / 1000);
    bool result = audio_feed_seek(feed, position, loop);

    pthread_mutex_lock(&state->mutex);
    audio_track_t* bgm = &state->bgm[channel];
    if(result && bgm->feed == feed) {
        bgm->delay = 0;
    }

    pthread_mutex_unlock(&state->mutex);
    audio_feed_close(feed);
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
