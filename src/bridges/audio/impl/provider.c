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
      lpThis->sMethods->methods[0] = audioProviderInit;
      lpThis->sMethods->methods[1] = audioProviderResume;
      lpThis->sMethods->methods[2] = audioProviderSetBGMVolume;
      lpThis->sMethods->methods[3] = audioProviderSetSFXVolume;
      lpThis->sMethods->methods[4] = audioProviderSupsend;
      lpThis->sMethods->methods[5] = audioProviderUpdate;
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
}

void audioProviderResume(void *instance)
{
  logV(TAG, "audioProviderResume()");
}

int audioProviderSetBGMVolume(void *instance, float volume)
{
  logV(TAG, "audioProviderSetBGMVolume()");
}

int audioProviderSetSFXVolume(void *instance, float volume)
{
  logV(TAG, "audioProviderSetSFXVolume()");
}

void audioProviderSupsend(void *instance)
{
  logV(TAG, "audioProviderSupsend()");
}

void audioProviderUpdate(void *instance)
{
  logV(TAG, "audioProviderUpdate()");
}
