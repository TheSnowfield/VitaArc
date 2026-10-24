#include <psp2/kernel/clib.h>
#include <psp2/kernel/sysmem.h>
#include <kubridge.h>

SceUID __real_sceKernelAllocMemBlock(const char* name, SceKernelMemBlockType type, SceSize size,
    SceKernelAllocMemBlockOpt* options);
SceUID __real_kuKernelAllocMemBlock(const char* name, SceKernelMemBlockType type, SceSize size,
    SceKernelAllocMemBlockKernelOpt* options);

SceUID __wrap_sceKernelAllocMemBlock(const char* name, SceKernelMemBlockType type, SceSize size,
    SceKernelAllocMemBlockOpt* options)
{
    SceUID block = __real_sceKernelAllocMemBlock(name, type, size, options);
    if(size >= 1024 * 1024) {
        void* base = NULL;
        if(block >= 0) sceKernelGetMemBlockBase(block, &base);
        sceClibPrintf("ARC MEMORY name=%s size=%u base=%p block=%08x\n", name, size, base, block);
    }

    return block;
}

SceUID __wrap_kuKernelAllocMemBlock(const char* name, SceKernelMemBlockType type, SceSize size,
    SceKernelAllocMemBlockKernelOpt* options)
{
    SceUID block = __real_kuKernelAllocMemBlock(name, type, size, options);
    if(block < 0 && options) {
        SceKernelMemBlockInfo info = {.size = sizeof(info)};
        int result = sceKernelGetMemBlockInfoByAddr((void*)options->field_C, &info);
        sceClibPrintf("ARC MEMORY map-fail target=%08x query=%08x base=%p size=%u\n",
            options->field_C, result, info.mappedBase, info.mappedSize);
    }

    return block;
}
