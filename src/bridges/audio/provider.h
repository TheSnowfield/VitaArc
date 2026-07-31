#ifndef __BRIDGES_AUDIO_PROVIDER_H
#define __BRIDGES_AUDIO_PROVIDER_H

#include <stdbool.h>
#include <stdint.h>
#include <abi/calling.h>

extern uintptr_t vita_audio_provider_vtable[];
extern uintptr_t vita_audio_provider_vtable_v2[];
extern uintptr_t vita_audio_provider_vtable_v3[];

void* audio_provider_construct(void);
int VITAARC_SOFTFP audio_provider_init(void* instance, float sfx_volume, float bgm_volume);
void audio_provider_destruct(void* instance);
void audio_provider_delete(void* instance);

void audio_provider_load_bgm(void* instance, const char* path, int channel);
void audio_provider_unload_bgm(void* instance, int channel);
void audio_provider_play_bgm(void* instance, bool loop, int channel, int delay_ms);
void audio_provider_set_bgm_paused(void* instance, bool paused, int channel);
int audio_provider_get_bgm_position(void* instance, int channel);
void audio_provider_set_bgm_position(void* instance, int position_ms, int channel);
void VITAARC_SOFTFP audio_provider_set_channel_bgm_volume(void* instance, float volume, int channel);
void VITAARC_SOFTFP audio_provider_set_bgm_volume(void* instance, float volume);

void audio_provider_preload_sfx(void* instance, const char* path);
void audio_provider_unload_sfx(void* instance, const char* path);
int audio_provider_play_sfx(void* instance, const char* path, int unused, bool force);
int VITAARC_SOFTFP audio_provider_play_sfx_v2(
    void* instance, const char* path, int loop_count, bool force, float volume);
int VITAARC_SOFTFP audio_provider_play_sfx_v3(void* instance, const char* path, bool loop, bool force, float volume);
void audio_provider_stop_sfx_v3(void* instance, const char* path);
float VITAARC_SOFTFP audio_provider_get_sfx_volume_v3(void* instance, const char* path);
void VITAARC_SOFTFP audio_provider_set_sfx_volume_v3(void* instance, const char* path, float volume);
void audio_provider_stop_sfx(void* instance, int handle);
void VITAARC_SOFTFP audio_provider_set_sfx_volume(void* instance, float volume);

void audio_provider_update(void* instance);
void audio_provider_suspend(void* instance);
void audio_provider_resume(void* instance);

#endif /* __BRIDGES_AUDIO_PROVIDER_H */
