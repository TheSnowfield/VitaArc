#ifndef __MOCK_PSP2_KERNEL_THREADMGR_H
#define __MOCK_PSP2_KERNEL_THREADMGR_H
#include <psp2/types.h>
#define SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT 0
typedef int (*SceKernelThreadEntry)(SceSize size, void* arguments);
SceUID sceKernelCreateThread(const char* name, SceKernelThreadEntry entry, int priority, SceSize stack,
    unsigned int attributes, int affinity, const void* options);
int sceKernelStartThread(SceUID thread, SceSize size, void* arguments);
int sceKernelWaitThreadEnd(SceUID thread, int* status, void* timeout);
int sceKernelDeleteThread(SceUID thread);
int sceKernelDelayThread(unsigned int microseconds);
#endif
