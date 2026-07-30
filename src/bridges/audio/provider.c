#include <stdlib.h>
#include <stdint.h>
#include "../../logcat/logcat.h"
#include "provider.h"

typedef struct IAMETHODS
{
  uintptr_t methods[6];
} IAMETHODS;

typedef struct IAMCLASS
{
  IAMETHODS *methods_table;
} IAMCLASS;

void *audio_provider_construct(void)
{
  log_v(TAG, "audio_provider_construct()");

  IAMCLASS *instance = malloc(sizeof(IAMCLASS));
  {
    instance->methods_table = malloc(sizeof(IAMETHODS));
    {
      instance->methods_table->methods[0] =
          (uintptr_t)&audio_provider_init;
      instance->methods_table->methods[1] =
          (uintptr_t)&audio_provider_resume;
      instance->methods_table->methods[2] =
          (uintptr_t)&audio_provider_set_bgm_volume;
      instance->methods_table->methods[3] =
          (uintptr_t)&audio_provider_set_sfx_volume;
      instance->methods_table->methods[4] =
          (uintptr_t)&audio_provider_suspend;
      instance->methods_table->methods[5] =
          (uintptr_t)&audio_provider_update;
    }
  }

  return instance;
}

void audio_provider_destruct(void *instance)
{
  (void)instance;
  log_v(TAG, "audio_provider_destruct()");
}

int audio_provider_init(void *instance, float x, float y)
{
  (void)instance;
  (void)x;
  (void)y;
  log_v(TAG, "audio_provider_init()");
  return 1;
}

void audio_provider_resume(void *instance)
{
  (void)instance;
  log_v(TAG, "audio_provider_resume()");
}

int audio_provider_set_bgm_volume(void *instance, float volume)
{
  (void)instance;
  (void)volume;
  log_v(TAG, "audio_provider_set_bgm_volume()");
  return 1;
}

int audio_provider_set_sfx_volume(void *instance, float volume)
{
  (void)instance;
  (void)volume;
  log_v(TAG, "audio_provider_set_sfx_volume()");
  return 1;
}

void audio_provider_suspend(void *instance)
{
  (void)instance;
  log_v(TAG, "audio_provider_suspend()");
}

void audio_provider_update(void *instance)
{
  (void)instance;
  log_v(TAG, "audio_provider_update()");
}
