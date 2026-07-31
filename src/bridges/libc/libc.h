#ifndef __BRIDGES_LIBC_IMPL_LIBC_H
#define __BRIDGES_LIBC_IMPL_LIBC_H

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "posix.h"
#include "file.h"
#include "cxx.h"
#include "memory.h"
#include "directory.h"
#include "guest_errno.h"

extern int* __errno(void);
static const uint32_t __stack_chk_guard = 0xDEADBEEF;

void bridge_stack_chk_fail();

void bridge_check_failed();

void bridge_assert2();

int bridge_dladdr(void* addr, void* info);

int bridge_dlclose(void* handle);

char* bridge_dlerror(void);

void* bridge_dlopen(const char* filename, int flags);

void* bridge_dlsym(void* handle, const char* symbol);

void __fortify_chk_fail(const char* msg, uint32_t event_tag);

uintptr_t bridge_gnu_unwind_find_exidx(uintptr_t pc, int* pcount);

size_t bridge_fread(void* _buf, size_t _size, size_t _n, FILE* _file);

size_t bridge_strlen(const char* _str);

#endif /* __BRIDGES_LIBC_IMPL_LIBC_H */
