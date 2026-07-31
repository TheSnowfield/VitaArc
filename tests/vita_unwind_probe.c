#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>

#include <config.h>
#include <loader.h>
#include <logcat/logcat.h>
#include <string.h>
#include "game_unwind_profile.h"

int main(void)
{
    log_begin(PATH_TO_LOGFILE);
    dynalib_t* game = loader_load_library(LIBRARY_LIBCOCOS2DCPP);
    if(!game) {
        sceClibPrintf("VitaArc unwind probe: game runtime load failed\n");
        sceKernelExitProcess(1);
        return 1;
    }

    dynalib_t* fixture = loader_load_library("app0:unwind-fixture.so");
    int (*test)(void) = fixture ? loader_get_proc_address(fixture, "fixture_throw_catch") : NULL;
    void (*set_personality)(void*) = fixture ? loader_get_proc_address(fixture, "fixture_set_personality") : NULL;
    uintptr_t personality = game->base + GAME_PERSONALITY_RVA;
    if(!test || !set_personality ||
        memcmp((void*)(personality & ~(uintptr_t)1), game_personality_profile, sizeof(game_personality_profile)) != 0) {
        sceClibPrintf("VitaArc unwind probe: exception fixture load failed\n");
        sceKernelExitProcess(2);
        return 2;
    }

    set_personality((void*)personality);
    int result = test();
    sceClibPrintf("VitaArc unwind probe: catch result=%d (42 plus one RAII cleanup)\n", result);
    sceClibPrintf("VitaArc unwind probe: exit=%d\n", result == 43 ? 0 : 3);
    /* The game runtime can retain exception TLS. Keep both code mappings alive
     * through process exit; this probe does not establish runtime unloading. */
    sceKernelExitProcess(result == 43 ? 0 : 3);
    return result == 43 ? 0 : 3;
}
