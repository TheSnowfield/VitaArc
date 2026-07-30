#ifndef __BRIDGES_AUDIO_PROVIDER_H
#define __BRIDGES_AUDIO_PROVIDER_H

void *audio_provider_construct(void);

void audio_provider_destruct(void *instance);

int audio_provider_init(void *instance, float x, float y);

void audio_provider_resume(void *instance);

int audio_provider_set_bgm_volume(void *instance, float volume);

int audio_provider_set_sfx_volume(void *instance, float volume);

void audio_provider_suspend(void *instance);

void audio_provider_update(void *instance);

#endif /* __BRIDGES_AUDIO_PROVIDER_H */
