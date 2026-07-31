#include <string.h>
#include <stddef.h>
#include <psp2/types.h>

#include "string.h"

const char* util_get_file_name(const char* file_path)
{
    if(!file_path) {
        return NULL;
    }

    const char* separator = strrchr(file_path, '/');
    if(!separator) {
        separator = strrchr(file_path, ':');
    }

    const char* name = separator ? separator + 1 : file_path;
    return *name ? name : NULL;
}

size_t util_uchar_len(const uint16_t* string)
{
    if(!string) {
        return -1;
    }

    for(const uint16_t* i = string;; ++i) {
        if(*i == 0) {
            return i - string;
        }
    }

    return -1;
}
