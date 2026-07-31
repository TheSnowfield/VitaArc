#ifndef __TESTS_MOCKS_PSP2_KERNEL_RNG_H
#define __TESTS_MOCKS_PSP2_KERNEL_RNG_H
#include <psp2/types.h>
int sceKernelGetRandomNumber(void* output, SceSize size);
#endif
