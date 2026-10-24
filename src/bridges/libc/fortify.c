#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "fortify.h"

char* bridge_strcpy_chk(char* output, const char* text, size_t size)
{
    size_t length = strlen(text);
    if(length >= size) {
        abort();
    }

    return memcpy(output, text, length + 1);
}

ssize_t bridge_read_chk(int fd, void* buffer, size_t count, size_t size)
{
    if(count > size || count > (SIZE_MAX >> 1)) {
        abort();
    }

    return bridge_read(fd, buffer, count);
}

static uint32_t* __word(int fd, void* set, size_t size)
{
    if(fd < 0 || fd >= 1024 || (size_t)fd / 8 >= size || !set) {
        abort();
    }

    if(size - (size_t)(fd / 32) * 4 < 4) {
        abort();
    }

    return (uint32_t*)set + fd / 32;
}

void bridge_fd_set_chk(int fd, void* set, size_t size)
{
    *__word(fd, set, size) |= UINT32_C(1) << (fd % 32);
}

void bridge_fd_clr_chk(int fd, void* set, size_t size)
{
    *__word(fd, set, size) &= ~(UINT32_C(1) << (fd % 32));
}
