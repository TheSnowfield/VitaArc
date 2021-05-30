#include <stdlib.h>
#include <string.h>

#include "../../common/define.h"
#include "../../utils/patcher.h"
#include "../../utils/debug.h"
#include "../../logcat/logcat.h"
#include "impl/provider.h"
#include "audio.h"

static const BRIDGEFUNC BRIDGE_AUDIO[] =
    {
        // {"_ZTI13AudioProvider", (uintptr_t)&audioProviderConstruct},
        {"_ZN13AudioProvider4initEff", (uintptr_t)&audioProviderInit},
        {"_ZN13AudioProvider6resumeEv", (uintptr_t)&audioProviderResume},
        {"_ZN13AudioProvider12setBGMVolumeEf", (uintptr_t)&audioProviderSetBGMVolume},
        {"_ZN13AudioProvider12setSFXVolumeEf", (uintptr_t)&audioProviderSetSFXVolume},
        {"_ZN13AudioProvider7suspendEv", (uintptr_t)&audioProviderSupsend},
        {"_ZN13AudioProvider6updateEv", (uintptr_t)&audioProviderUpdate},
        {"_ZN13AudioProviderD2Ev", (uintptr_t)&audioProviderDestruct},
};

typedef void (*AudioManagerConstruct)(void *);
typedef bool (*AudioManagerInit)(void *, void *);
typedef void (*AudioManagerDestruct)(void *);

static AudioManagerConstruct lpfnAudioManagerConstruct = NULL;
static AudioManagerInit lpfnAudioManagerInit = NULL;

void *audioManagerCreate(void *audioManager, void *audioProvider)
{
  // Allocate space
  void *lpAudioManagerThis = malloc(0x268u);
  memset(lpAudioManagerThis, 0x00, 0x268u);

  // Construct audio manager
  logV(TAG, "lpAudioManager => 0x%08X", lpAudioManagerThis);
  logV(TAG, "lpfnAudioManagerInit => 0x%08X", lpfnAudioManagerInit);
  logV(TAG, "lpfnAudioManagerConstruct => 0x%08X", lpfnAudioManagerConstruct);
  lpfnAudioManagerConstruct(lpAudioManagerThis);
  {
    logV(TAG, "Audio manager created");

    // Initlalize audio manager
    if (!lpfnAudioManagerInit(lpAudioManagerThis, audioProviderConstruct()))
    {
      logE(TAG, "Audio manager init failed");

      // Destruct it if fail
      AudioManagerDestruct lpfnAudioManagerDestruct =
          (AudioManagerDestruct)(*(uintptr_t *)(*((uintptr_t *)lpAudioManagerThis) + 4));
      lpfnAudioManagerDestruct(lpAudioManagerThis);
      lpAudioManagerThis = NULL;
    }
  }

  logV(TAG, "Audio manager => 0x%08X", lpAudioManagerThis);
  return lpAudioManagerThis;
}

void bridgeAudioProvider(HSOLIB hSoLibrary)
{
  patchSymbols(hSoLibrary, BRIDGE_AUDIO, sizeof(BRIDGE_AUDIO) / sizeof(BRIDGEFUNC));

  lpfnAudioManagerConstruct =
      (AudioManagerConstruct)solibGetProcAddress(hSoLibrary, "_ZN12AudioManagerC2Ev");
  lpfnAudioManagerInit =
      (AudioManagerInit)solibGetProcAddress(hSoLibrary, "_ZN12AudioManager4initEP13AudioProvider");

  hookStubProc(hSoLibrary, 0x38FD1C, &audioManagerCreate);
}
