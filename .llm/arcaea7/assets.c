#include <stdio.h>
#include <string.h>
#include <psp2/kernel/clib.h>
#include <android/assetmgr.h>

static unsigned __opens;
static unsigned __misses;

FILE* __real_fopen(const char* path, const char* mode);
a_asset_t* __real_a_asset_manager_open(a_asset_manager_t* manager, const char* name, int mode);

FILE* __wrap_fopen(const char* path, const char* mode)
{
    FILE* file = __real_fopen(path, mode);
    if(path && !strncmp(path, "app0:package/assets/", 20)) {
        ++__opens;
        __misses += file == NULL;
    }

    return file;
}

a_asset_t* __wrap_a_asset_manager_open(a_asset_manager_t* manager, const char* name, int mode)
{
    a_asset_t* asset = __real_a_asset_manager_open(manager, name, mode);
    if(name && strstr(name, "0_icon.png")) {
        sceClibPrintf("ARC ASSET name=%s result=%s caller=%p\n", name, asset ? "opened" : "absent",
            __builtin_return_address(0));
    }

    return asset;
}

void arc_asset_profile(void)
{
    sceClibPrintf("ARC ASSET streams=%u misses=%u\n", __opens, __misses);
}
