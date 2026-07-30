#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <loader.h>
#include <patcher.h>
#include "../../utils/debug.h"
#include "../../logcat/logcat.h"
#include "provider.h"
#include "manager.h"

typedef void (*AudioManagerConstruct)(void *);
typedef bool (*AudioManagerInit)(void *, void *);
typedef void (*AudioManagerDestruct)(void *);

static AudioManagerConstruct __audio_manager_construct = NULL;
static AudioManagerInit __audio_manager_init = NULL;

void *audio_manager_create(void *audio_manager, void *audio_provider)
{
  (void)audio_manager;
  (void)audio_provider;

  // Allocate space
  void *audio_manager_instance = malloc(0x268u);
  memset(audio_manager_instance, 0x00, 0x268u);

  // Construct audio manager
  log_v(TAG, "audio_manager => 0x%08X", audio_manager_instance);
  log_v(TAG, "audio_manager_init => 0x%08X", __audio_manager_init);
  log_v(TAG, "audio_manager_construct => 0x%08X",
        __audio_manager_construct);
  __audio_manager_construct(audio_manager_instance);
  {
    log_v(TAG, "Audio manager created");

    // Initlalize audio manager
    if (!__audio_manager_init(audio_manager_instance,
                              audio_provider_construct()))
    {
      log_e(TAG, "Audio manager init failed");

      // Destruct it if fail
      AudioManagerDestruct audio_manager_destruct =
          (AudioManagerDestruct)(
              *(uintptr_t *)(*((uintptr_t *)audio_manager_instance) + 4));
      audio_manager_destruct(audio_manager_instance);
      audio_manager_instance = NULL;
    }
  }

  log_v(TAG, "Audio manager => 0x%08X", audio_manager_instance);
  return audio_manager_instance;
}

void bridge_audio_provider(dynalib_t *library)
{
  __audio_manager_construct =
      (AudioManagerConstruct)loader_get_proc_address(
          library, "_ZN12AudioManagerC2Ev");
  __audio_manager_init =
      (AudioManagerInit)loader_get_proc_address(
          library, "_ZN12AudioManager4initEP13AudioProvider");

  hook_stub_proc(library, 0x38FD1C, &audio_manager_create);
}
