#ifndef _BRIDGE_AUDIO_PROVIDERIMPL_
#define _BRIDGE_AUDIO_PROVIDERIMPL_

void *audioProviderConstruct();

void audioProviderDestruct(void *instance);

int audioProviderInit(void *instance, float x, float y);

void audioProviderResume(void *instance);

int audioProviderSetBGMVolume(void *instance, float volume);

int audioProviderSetSFXVolume(void *instance, float volume);

void audioProviderSupsend(void *instance);

void audioProviderUpdate(void *instance);

#endif /* _BRIDGE_AUDIO_PROVIDERIMPL_ */
