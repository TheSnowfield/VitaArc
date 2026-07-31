#include <stdlib.h>

#include "../../logcat/logcat.h"
#include "backend.h"
#include "provider.h"

typedef struct audio_provider_t {
    uintptr_t* vtable;
    audio_t* state;
} audio_provider_t;

/*
 * Itanium C++ ABI vtable. The imported _ZTV symbol points at the two-word
 * header; cocos_android_app_init stores _ZTV + 8 in the provider object.
 */
uintptr_t vita_audio_provider_vtable[] = {
    0,
    0,
    (uintptr_t)audio_provider_init,
    (uintptr_t)audio_provider_destruct,
    (uintptr_t)audio_provider_delete,
    (uintptr_t)audio_provider_load_bgm,
    (uintptr_t)audio_provider_unload_bgm,
    (uintptr_t)audio_provider_play_bgm,
    (uintptr_t)audio_provider_set_bgm_paused,
    (uintptr_t)audio_provider_get_bgm_position,
    (uintptr_t)audio_provider_set_bgm_position,
    (uintptr_t)audio_provider_set_channel_bgm_volume,
    (uintptr_t)audio_provider_preload_sfx,
    (uintptr_t)audio_provider_unload_sfx,
    (uintptr_t)audio_provider_play_sfx,
    (uintptr_t)audio_provider_stop_sfx,
    (uintptr_t)audio_provider_set_sfx_volume,
    (uintptr_t)audio_provider_update,
    (uintptr_t)audio_provider_suspend,
    (uintptr_t)audio_provider_resume,
};

/* Exact slot orders from each APK's libfmodProvider.so relocations. The old
 * table stays available for 1.6.1c; newer slots cannot be appended to it
 * because insertions move update/suspend/resume to different offsets. */
#define PROVIDER_COMMON                                                                                                \
    0, 0, (uintptr_t)audio_provider_init, (uintptr_t)audio_provider_destruct, (uintptr_t)audio_provider_delete,        \
        (uintptr_t)audio_provider_load_bgm, (uintptr_t)audio_provider_unload_bgm, (uintptr_t)audio_provider_play_bgm,  \
        (uintptr_t)audio_provider_set_bgm_paused, (uintptr_t)audio_provider_get_bgm_position,                          \
        (uintptr_t)audio_provider_set_bgm_position, (uintptr_t)audio_provider_set_channel_bgm_volume,                  \
        (uintptr_t)audio_provider_preload_sfx, (uintptr_t)audio_provider_unload_sfx
uintptr_t vita_audio_provider_vtable_v2[] = {PROVIDER_COMMON,
    (uintptr_t)audio_provider_play_sfx_v2,
    (uintptr_t)audio_provider_stop_sfx,
    (uintptr_t)audio_provider_set_sfx_volume,
    (uintptr_t)audio_provider_play_sfx_v2,
    (uintptr_t)audio_provider_update,
    (uintptr_t)audio_provider_suspend,
    (uintptr_t)audio_provider_resume};

uintptr_t vita_audio_provider_vtable_v3[] = {PROVIDER_COMMON,
    (uintptr_t)audio_provider_play_sfx_v3,
    (uintptr_t)audio_provider_stop_sfx_v3,
    (uintptr_t)audio_provider_get_sfx_volume_v3,
    (uintptr_t)audio_provider_set_sfx_volume_v3,
    (uintptr_t)audio_provider_set_sfx_volume,
    (uintptr_t)audio_provider_play_sfx_v3,
    (uintptr_t)audio_provider_update,
    (uintptr_t)audio_provider_suspend,
    (uintptr_t)audio_provider_resume};

#undef PROVIDER_COMMON

static audio_provider_t* get_provider(void* instance)
{
    return (audio_provider_t*)instance;
}

void* audio_provider_construct(void)
{
    audio_provider_t* provider = calloc(1, sizeof(*provider));
    if(provider) {
        provider->vtable = &vita_audio_provider_vtable[2];
    }

    return provider;
}

int VITAARC_SOFTFP audio_provider_init(void* instance, float sfx_volume, float bgm_volume)
{
    audio_provider_t* provider = get_provider(instance);
    if(!provider) {
        return 0;
    }

    if(provider->state) {
        vita_audio_set_sfx_volume(provider->state, sfx_volume);
        vita_audio_set_bgm_volume(provider->state, bgm_volume);
        return 1;
    }

    provider->state = vita_audio_create(sfx_volume, bgm_volume);
    if(!provider->state) {
        log_e(TAG, "Unable to initialize Vita AudioProvider");
        return 0;
    }

    log_i(TAG, "Vita AudioProvider initialized");
    return 1;
}

void audio_provider_destruct(void* instance)
{
    audio_provider_t* provider = get_provider(instance);
    if(!provider) {
        return;
    }

    vita_audio_destroy(provider->state);
    provider->state = NULL;
}

void audio_provider_delete(void* instance)
{
    audio_provider_destruct(instance);
    free(instance);
}

void audio_provider_load_bgm(void* instance, const char* path, int channel)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_load_bgm(provider->state, path, channel);
    }
}

void audio_provider_unload_bgm(void* instance, int channel)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_unload_bgm(provider->state, channel);
    }
}

void audio_provider_play_bgm(void* instance, bool loop, int channel, int delay_ms)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_play_bgm(provider->state, loop, channel, delay_ms);
    }
}

void audio_provider_set_bgm_paused(void* instance, bool paused, int channel)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_set_bgm_paused(provider->state, paused, channel);
    }
}

int audio_provider_get_bgm_position(void* instance, int channel)
{
    audio_provider_t* provider = get_provider(instance);
    return provider ? vita_audio_get_bgm_position(provider->state, channel) : -1;
}

void audio_provider_set_bgm_position(void* instance, int position_ms, int channel)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && !vita_audio_seek_bgm(provider->state, position_ms, channel)) {
        log_w(TAG, "BGM seek failed: channel=%d, position=%d ms", channel, position_ms);
    }
}

void VITAARC_SOFTFP audio_provider_set_channel_bgm_volume(void* instance, float volume, int channel)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider) {
        vita_audio_set_channel_bgm_volume(provider->state, volume, channel);
    }
}

void VITAARC_SOFTFP audio_provider_set_bgm_volume(void* instance, float volume)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_set_bgm_volume(provider->state, volume);
    }
}

void audio_provider_preload_sfx(void* instance, const char* path)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_preload_sfx(provider->state, path);
    }
}

void audio_provider_unload_sfx(void* instance, const char* path)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_unload_sfx(provider->state, path);
    }
}

int audio_provider_play_sfx(void* instance, const char* path, int unused, bool force)
{
    audio_provider_t* provider = get_provider(instance);
    if(!provider || !provider->state) {
        return -1;
    }

    return vita_audio_play_sfx(provider->state, path, unused, force);
}

void audio_provider_stop_sfx(void* instance, int handle)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_stop_sfx(provider->state, handle);
    }
}

int VITAARC_SOFTFP audio_provider_play_sfx_v2(
    void* instance, const char* path, int loop_count, bool force, float volume)
{
    audio_provider_t* provider = get_provider(instance);
    return provider && provider->state ? vita_audio_play_sfx_ex(provider->state, path, loop_count, force, volume) : -1;
}

int VITAARC_SOFTFP audio_provider_play_sfx_v3(void* instance, const char* path, bool loop, bool force, float volume)
{
    return audio_provider_play_sfx_v2(instance, path, loop ? -1 : 0, force, volume);
}

void audio_provider_stop_sfx_v3(void* instance, const char* path)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider)
        vita_audio_stop_sfx_path(provider->state, path);
}

float VITAARC_SOFTFP audio_provider_get_sfx_volume_v3(void* instance, const char* path)
{
    audio_provider_t* provider = get_provider(instance);
    return provider ? vita_audio_get_sfx_path_volume(provider->state, path) : 0.0f;
}

void VITAARC_SOFTFP audio_provider_set_sfx_volume_v3(void* instance, const char* path, float volume)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider)
        vita_audio_set_sfx_path_volume(provider->state, path, volume);
}

void VITAARC_SOFTFP audio_provider_set_sfx_volume(void* instance, float volume)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_set_sfx_volume(provider->state, volume);
    }
}

void audio_provider_update(void* instance)
{
    (void)instance;
}

void audio_provider_suspend(void* instance)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_suspend(provider->state);
    }
}

void audio_provider_resume(void* instance)
{
    audio_provider_t* provider = get_provider(instance);
    if(provider && provider->state) {
        vita_audio_resume(provider->state);
    }
}
