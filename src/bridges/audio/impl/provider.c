#include <stdlib.h>
#include <stdint.h>
#include "../../../logcat/logcat.h"
#include "provider.h"

typedef struct IAMETHODS
{
  uintptr_t methods[6];
} IAMETHODS;

typedef struct IAMCLASS
{
  IAMETHODS *sMethods;
} IAMCLASS;

void *audioProviderConstruct()
{
  logV(TAG, "audioProviderConstruct()");

  IAMCLASS *lpThis = malloc(sizeof(IAMCLASS));
  {
    lpThis->sMethods = malloc(sizeof(IAMETHODS));
    {
      lpThis->sMethods->methods[0] = (uintptr_t)&audioProviderInit;
      lpThis->sMethods->methods[1] = (uintptr_t)&audioProviderResume;
      lpThis->sMethods->methods[2] = (uintptr_t)&audioProviderSetBGMVolume;
      lpThis->sMethods->methods[3] = (uintptr_t)&audioProviderSetSFXVolume;
      lpThis->sMethods->methods[4] = (uintptr_t)&audioProviderSupsend;
      lpThis->sMethods->methods[5] = (uintptr_t)&audioProviderUpdate;
    }
  }

  return lpThis;
}

void audioProviderDestruct(void *instance)
{
  logV(TAG, "audioProviderDestruct()");
}

int audioProviderInit(void *instance, float x, float y)
{
  logV(TAG, "audioProviderinit()");
  return 1;
}

void audioProviderResume(void *instance)
{
  logV(TAG, "audioProviderResume()");
}

int audioProviderSetBGMVolume(void *instance, float volume)
{
  logV(TAG, "audioProviderSetBGMVolume()");
  return 1;
}

int audioProviderSetSFXVolume(void *instance, float volume)
{
  logV(TAG, "audioProviderSetSFXVolume()");
  return 1;
}

void audioProviderSupsend(void *instance)
{
  logV(TAG, "audioProviderSupsend()");
}

void audioProviderUpdate(void *instance)
{
  logV(TAG, "audioProviderUpdate()");
}
