#include <stdlib.h>
#include <string.h>
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <logcat/logcat.h>
#include "mp3.h"
#include "state.h"

static int audio_thread(SceSize argument_size, void* arguments)
{
    audio_t* state = NULL;
    if(argument_size == sizeof(state) && arguments) {
        memcpy(&state, arguments, sizeof(state));
    }

    if(!state) {
        return -1;
    }

    int16_t output[VITA_AUDIO_GRAIN * VITA_AUDIO_CHANNELS];
    while(atomic_load(&state->running)) {
        vita_audio_render(state, output, VITA_AUDIO_GRAIN);
        int result = sceAudioOutOutput(state->port, output);
        if(result < 0) {
            log_e(TAG, "sceAudioOutOutput failed: 0x%08X", result);
            sceKernelDelayThread(1000);
        }
    }

    return 0;
}

audio_t* vita_audio_create(float sfx_volume, float bgm_volume)
{
    audio_t* state = calloc(1, sizeof(*state));
    if(!state) {
        return NULL;
    }

    state->port = -1;
    state->thread = -1;
    state->serial = 1;
    state->sfx_volume = audio_gain(sfx_volume);
    state->bgm_volume = audio_gain(bgm_volume);
    atomic_init(&state->running, false);
    if(pthread_mutex_init(&state->mutex, NULL) != 0) {
        free(state);
        return NULL;
    }

    state->has_mp3 = vita_mp3_library_init(VITA_AUDIO_BGM_CHANNELS);
    if(!state->has_mp3) {
        log_e(TAG, "sceAudiodec MP3 initialization failed; Vorbis playback remains available");
    }

    state->port = sceAudioOutOpenPort(
        SCE_AUDIO_OUT_PORT_TYPE_MAIN, VITA_AUDIO_GRAIN, VITA_AUDIO_SAMPLE_RATE, SCE_AUDIO_OUT_MODE_STEREO);
    if(state->port < 0) {
        log_e(TAG, "sceAudioOutOpenPort failed: 0x%08X", state->port);
        vita_audio_destroy(state);
        return NULL;
    }

    atomic_store(&state->running, true);
    state->thread = sceKernelCreateThread(
        "VitaArcAudio", audio_thread, 0x10000100, 0x10000, 0, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, NULL);
    if(state->thread < 0) {
        log_e(TAG, "sceKernelCreateThread(audio) failed: 0x%08X", state->thread);
        vita_audio_destroy(state);
        return NULL;
    }

    audio_t* thread_state = state;
    int result = sceKernelStartThread(state->thread, sizeof(thread_state), &thread_state);
    if(result < 0) {
        log_e(TAG, "sceKernelStartThread(audio) failed: 0x%08X", result);
        vita_audio_destroy(state);
        return NULL;
    }

    state->started = true;

    log_i(TAG, "Vita audio backend initialized on port %d", state->port);
    return state;
}

void vita_audio_destroy(audio_t* state)
{
    if(!state) {
        return;
    }

    atomic_store(&state->running, false);
    if(state->thread >= 0) {
        if(state->started) {
            sceKernelWaitThreadEnd(state->thread, NULL, NULL);
        }

        sceKernelDeleteThread(state->thread);
    }

    if(state->port >= 0) {
        sceAudioOutReleasePort(state->port);
    }

    for(uint32_t i = 0; i < VITA_AUDIO_BGM_CHANNELS; ++i) {
        vita_audio_stream_close(state->bgm[i].stream);
    }

    for(uint32_t i = 0; i < VITA_AUDIO_MAX_SFX; ++i) {
        if(state->sfx[i].used) {
            vita_wav_free(&state->sfx[i].pcm);
            free(state->sfx[i].path);
        }
    }

    if(state->has_mp3) {
        vita_mp3_library_term();
    }

    pthread_mutex_destroy(&state->mutex);
    free(state);
}

void vita_audio_suspend(audio_t* state)
{
    if(!state) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    state->suspended = true;
    pthread_mutex_unlock(&state->mutex);
}

void vita_audio_resume(audio_t* state)
{
    if(!state) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    state->suspended = false;
    pthread_mutex_unlock(&state->mutex);
}
