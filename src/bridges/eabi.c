#include <common/define.h>
#include <utils/patcher.h>
#include <logcat/logcat.h>
#include "eabi.h"

extern void *__aeabi_d2uiz;
extern void *__aeabi_dmul;
extern void *__aeabi_dsub;
extern void *__aeabi_f2ulz;
extern void *__aeabi_l2d;
extern void *__aeabi_l2f;
extern void *__aeabi_memclr;
extern void *__aeabi_memclr4;
extern void *__aeabi_memclr8;
extern void *__aeabi_memcpy;
extern void *__aeabi_memcpy4;
extern void *__aeabi_memcpy8;
extern void *__aeabi_memmove;
extern void *__aeabi_memmove4;
extern void *__aeabi_memset;
extern void *__aeabi_memset4;
extern void *__aeabi_memset8;
extern void *__aeabi_ui2d;
extern void *__aeabi_ul2d;

static const BRIDGEFUNC BRIDGE_EABI[] =
    {
        {"__aeabi_d2uiz", (uintptr_t)&__aeabi_d2uiz},
        {"__aeabi_dmul", (uintptr_t)&__aeabi_dmul},
        {"__aeabi_dsub", (uintptr_t)&__aeabi_dsub},
        {"__aeabi_f2ulz", (uintptr_t)&__aeabi_f2ulz},
        {"__aeabi_l2d", (uintptr_t)&__aeabi_l2d},
        {"__aeabi_l2f", (uintptr_t)&__aeabi_l2f},
        {"__aeabi_memclr", (uintptr_t)&__aeabi_memclr},
        {"__aeabi_memclr4", (uintptr_t)&__aeabi_memclr4},
        {"__aeabi_memclr8", (uintptr_t)&__aeabi_memclr8},
        {"__aeabi_memcpy", (uintptr_t)&__aeabi_memcpy},
        {"__aeabi_memcpy4", (uintptr_t)&__aeabi_memcpy4},
        {"__aeabi_memcpy8", (uintptr_t)&__aeabi_memcpy8},
        {"__aeabi_memmove", (uintptr_t)&__aeabi_memmove},
        {"__aeabi_memmove4", (uintptr_t)&__aeabi_memmove4},
        {"__aeabi_memset", (uintptr_t)&__aeabi_memset},
        {"__aeabi_memset4", (uintptr_t)&__aeabi_memset4},
        {"__aeabi_memset8", (uintptr_t)&__aeabi_memset8},
        {"__aeabi_ui2d", (uintptr_t)&__aeabi_ui2d},
        {"__aeabi_ul2d", (uintptr_t)&__aeabi_ul2d}};

void bridgePatchEABI(HSOLIB hSoLibrary)
{
    patchSymbols(hSoLibrary, BRIDGE_EABI, sizeof(BRIDGE_EABI) / sizeof(BRIDGEFUNC));
}
