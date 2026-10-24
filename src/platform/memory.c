#include <limits.h>
#include <malloc.h>
#include <psp2/kernel/sysmem.h>
#include <vitaGL.h>

#include <logcat/logcat.h>
#include "memory.h"

#ifndef VITAARC_HEAP_MB
#define VITAARC_HEAP_MB 64
#endif

#ifndef VITAARC_GPU_RAM_MB
#define VITAARC_GPU_RAM_MB 32
#endif

#ifndef VITAARC_GPU_STREAM_MB
#define VITAARC_GPU_STREAM_MB 6
#endif

#define MIB (1024u * 1024u)
#define MEMORY_INTERVAL UINT64_C(30000000)

/**
 * @brief Newlib reads this reservation before running constructors.
 */
unsigned int _newlib_heap_size_user = VITAARC_HEAP_MB * MIB;

typedef struct {
    unsigned heap;
    unsigned ram;
    unsigned vram;
} memory_peak_t;

static memory_peak_t __peak;
static uint64_t __next;
static bool __ready;

bool vita_memory_setup(void)
{
    SceKernelFreeMemorySizeInfo free = {.size = sizeof(free)};
    int result = sceKernelGetFreeMemorySize(&free);
    if(result < 0) {
        log_e(TAG, "Cannot query graphics memory budget: 0x%08X", (unsigned)result);
        return false;
    }

    unsigned ram = VITAARC_GPU_RAM_MB * MIB;
    if(free.size_user <= 0 || (unsigned)free.size_user <= ram) {
        log_e(TAG, "Insufficient graphics RAM: free=%d requested=%u", free.size_user, ram);
        return false;
    }

    if(free.size_cdram <= 0) {
        log_e(TAG, "Graphics VRAM is unavailable");
        return false;
    }

    /* vitaGL creates GXM resources before calculating its pools. Preserve a
     * fixed RAM reserve instead of claiming all remaining process memory.
     * PHYCONT and common-dialog memory remain available to their owners. */
    int reserve = free.size_user - (int)ram;
    vglSetCircularPoolSize(VITAARC_GPU_STREAM_MB * MIB);
    if(vglInitWithCustomThreshold(0, 960, 544, reserve, 0, INT_MAX, INT_MAX, SCE_GXM_MULTISAMPLE_NONE)) {
        log_w(TAG, "VitaGL selected a fallback display resolution");
    }

    __ready = true;
    log_i(TAG,
        "Memory budget: heap=%u MiB graphics RAM<=%u MiB stream=%u MiB",
        VITAARC_HEAP_MB,
        VITAARC_GPU_RAM_MB,
        VITAARC_GPU_STREAM_MB);
    return true;
}

static void __pool(const char* name, vglMemType type, unsigned* peak)
{
    unsigned total = vglMemTotal(type);
    unsigned free = vglMemFree(type);
    unsigned used = free <= total ? total - free : 0;
    if(used > *peak) {
        *peak = used;
    }

    log_i(TAG,
        "Graphics %s: used=%u free=%u reserved=%u peak=%u KiB",
        name,
        used / 1024,
        free / 1024,
        total / 1024,
        *peak / 1024);
}

void vita_memory_log(const char* stage)
{
    struct mallinfo heap = mallinfo();
    unsigned used = (unsigned)heap.uordblks;
    if(used > __peak.heap) {
        __peak.heap = used;
    }

    log_i(TAG,
        "Memory %s: heap used=%u reserved=%u arena=%u peak=%u KiB",
        stage,
        used / 1024,
        _newlib_heap_size_user / 1024,
        (unsigned)heap.arena / 1024,
        __peak.heap / 1024);
    if(__ready) {
        __pool("RAM", VGL_MEM_RAM, &__peak.ram);
        __pool("VRAM", VGL_MEM_VRAM, &__peak.vram);
    }

    SceKernelFreeMemorySizeInfo free = {.size = sizeof(free)};
    if(sceKernelGetFreeMemorySize(&free) >= 0) {
        log_i(TAG,
            "System free: RAM=%u VRAM=%u PHYCONT=%u KiB",
            (unsigned)free.size_user / 1024,
            (unsigned)free.size_cdram / 1024,
            (unsigned)free.size_phycont / 1024);
    }

    log_flush();
}

void vita_memory_tick(uint64_t now)
{
    if(now < __next) {
        return;
    }

    __next = now + MEMORY_INTERVAL;
    vita_memory_log("runtime");
}
