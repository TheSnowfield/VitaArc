#include <string.h>
#include <wchar.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

#include <psp2/kernel/rng.h>

#include <logcat/logcat.h>
#include <config.h>
#include <loader.h>
#include <utils/fs.h>
#include <utils/string.h>
#include "libc.h"

static char __dl_error[128];

void bridge_stack_chk_fail()
{
    log_f(TAG, "__stack_chk_fail hited. Program exit.");
    exit(-1);
}

void bridge_check_failed()
{
    log_f(TAG, "__check_failed hited. Program exit.");
    exit(-1);
}

void bridge_assert2()
{
    log_f(TAG, "__assert2 hited. Program exit.");
    exit(-1);
}

int bridge_dladdr(void* addr, void* info)
{
    log_w(TAG, "Unsupported symbol 'dladdr' called.");
    return 0;
}

int bridge_dlclose(void* handle)
{
    if(handle == (void*)-1) {
        return 0;
    }

    if(!handle) {
        snprintf(__dl_error, sizeof(__dl_error), "invalid library handle");
        return -1;
    }

    loader_free_library((dynalib_t*)handle);
    __dl_error[0] = '\0';
    return 0;
}

char* bridge_dlerror(void)
{
    return __dl_error[0] ? __dl_error : NULL;
}

void* bridge_dlopen(const char* filename, int flags)
{
    (void)flags;
    if(!filename) {
        __dl_error[0] = '\0';
        return (void*)-1;
    }

    const char* name = util_get_file_name(filename);
    dynalib_t* loaded = loader_find_library(name);
    if(loaded) {
        __dl_error[0] = '\0';
        return loader_clone_handle(loaded);
    }

    char path[256];
    snprintf(path, sizeof(path), "%s%s", PATH_TO_LIBRARY, name);
    if(!util_file_exists(path)) {
        snprintf(__dl_error, sizeof(__dl_error), "library not found: %s", name);
        return NULL;
    }

    dynalib_t* library = loader_load_library(path);
    if(!library) {
        snprintf(__dl_error, sizeof(__dl_error), "could not load: %s", name);
        return NULL;
    }

    __dl_error[0] = '\0';
    return library;
}

void* bridge_dlsym(void* handle, const char* symbol)
{
    if(!symbol) {
        snprintf(__dl_error, sizeof(__dl_error), "invalid symbol name");
        return NULL;
    }

    void* address;
    if(!handle || handle == (void*)-1) {
        address = loader_get_global_proc_address(symbol);
    } else {
        address = loader_get_proc_address((dynalib_t*)handle, symbol);
    }

    if(!address) {
        snprintf(__dl_error, sizeof(__dl_error), "symbol not found: %s", symbol);
        return NULL;
    }

    __dl_error[0] = '\0';
    return address;
}

void __fortify_chk_fail(const char* msg, uint32_t event_tag)
{
    log_f("bionic", msg);
    exit(-1);
}

uintptr_t bridge_gnu_unwind_find_exidx(uintptr_t pc, int* pcount)
{
    uintptr_t table = loader_find_exidx(pc, pcount);
    if(table || !pcount) {
        return table;
    }

    /* C callbacks and native runtime frames use the main executable's table.
     * Loaded DSOs retain their own PREL31 entries at their mapped addresses. */
    extern const unsigned char __exidx_start[], __exidx_end[];
    extern void _start(void);
    pc &= ~(uintptr_t)1;
    if(pc >= ((uintptr_t)_start & ~(uintptr_t)1) && pc < (uintptr_t)__exidx_start) {
        *pcount = (int)((__exidx_end - __exidx_start) / 8);
        return (uintptr_t)__exidx_start;
    }

    return 0;
}

size_t bridge_fread(void* _buf, size_t _size, size_t _n, FILE* _file)
{
    log_v(TAG, "Called _fread(0x%08X, %d, %d, 0x%08X)", _buf, _size, _n, _file);
    return fread(_buf, _size, _n, _file);
}

size_t bridge_strlen(const char* _str)
{
    log_v(TAG, "Called _strlen(\"%s\")", _str);
    return strlen(_str);
}
