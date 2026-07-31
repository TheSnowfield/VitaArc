#ifndef __BRIDGES_LIBC_CXX_H
#define __BRIDGES_LIBC_CXX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    int bridge_cxa_atexit(void (*callback)(void*), void* argument, void* dso);
    int bridge_aeabi_atexit(void* argument, void (*callback)(void*), void* dso);
    void bridge_cxa_finalize(void* dso);
    void bridge_cxa_finalize_range(uintptr_t base, size_t size);
    void bridge_exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}

#endif

#endif
