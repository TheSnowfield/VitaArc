#ifndef __BRIDGES_AUDIO_BACKEND_H
#define __BRIDGES_AUDIO_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

#define VITA_AUDIO_SAMPLE_RATE 48000
#define VITA_AUDIO_BLOCK_FRAMES 1024
#define VITA_AUDIO_BGM_CHANNELS 3

typedef struct audio_t audio_t;

audio_t* vita_audio_create(float sfx_volume, float bgm_volume);
void vita_audio_destroy(audio_t* state);

bool vita_audio_load_bgm(audio_t* state, const char* path, int channel);
void vita_audio_unload_bgm(audio_t* state, int channel);
void vita_audio_play_bgm(audio_t* state, bool loop, int channel, int delay_ms);
void vita_audio_set_bgm_paused(audio_t* state, bool paused, int channel);
void vita_audio_set_bgm_volume(audio_t* state, float volume);
int vita_audio_get_bgm_position(audio_t* state, int channel);
bool vita_audio_seek_bgm(audio_t* state, int position_ms, int channel);
void vita_audio_set_channel_bgm_volume(audio_t* state, float volume, int channel);
bool vita_audio_render(audio_t* state, int16_t* output, uint32_t frames);

bool vita_audio_preload_sfx(audio_t* state, const char* path);
void vita_audio_unload_sfx(audio_t* state, const char* path);
int vita_audio_play_sfx(audio_t* state, const char* path, int unused, bool force);
int vita_audio_play_sfx_ex(audio_t* state, const char* path, int loop_count, bool force, float volume);
void vita_audio_stop_sfx_path(audio_t* state, const char* path);
float vita_audio_get_sfx_path_volume(audio_t* state, const char* path);
void vita_audio_set_sfx_path_volume(audio_t* state, const char* path, float volume);
void vita_audio_stop_sfx(audio_t* state, int handle);
void vita_audio_set_sfx_volume(audio_t* state, float volume);

void vita_audio_suspend(audio_t* state);
void vita_audio_resume(audio_t* state);

#endif /* __BRIDGES_AUDIO_BACKEND_H */
