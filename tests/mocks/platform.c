#include <assert.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <loader.h>
#include <logcat/logcat.h>

const char* mock_missing_export;
_Thread_local char mock_last_info[256];

void* loader_get_proc_address(dynalib_t* library, const char* name)
{
    if(!library || !library->image) {
        return NULL;
    }

    if(mock_missing_export && strcmp(name, mock_missing_export) == 0) {
        return NULL;
    }

    return dlsym(library->image, name);
}

int sceKernelGetRandomNumber(void* output, uint32_t size)
{
    static uint32_t __state = 0x12345678;
    assert(size <= 64);
    unsigned char* bytes = output;
    for(uint32_t index = 0; index < size; ++index) {
        __state = __state * 1664525 + 1013904223;
        bytes[index] = (unsigned char)(__state >> 24);
    }

    return 0;
}

void log_v(const char* tag, const char* format, ...)
{
    (void)tag;
    (void)format;
}

void log_i(const char* tag, const char* format, ...)
{
    (void)tag;
    va_list args;
    va_start(args, format);
    vsnprintf(mock_last_info, sizeof(mock_last_info), format, args);
    va_end(args);
}

#define DEFINE_LOG(name)                                                                                               \
    void name(const char* tag, const char* format, ...)                                                                \
    {                                                                                                                  \
        va_list args;                                                                                                  \
        va_start(args, format);                                                                                        \
        fprintf(stderr, "%s: ", tag);                                                                                  \
        vfprintf(stderr, format, args);                                                                                \
        fputc('\n', stderr);                                                                                           \
        va_end(args);                                                                                                  \
    }

DEFINE_LOG(log_w)
DEFINE_LOG(log_e)
DEFINE_LOG(log_f)
