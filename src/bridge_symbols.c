#define _GNU_SOURCE

#include <string.h>

#include <generated/bridge_symbols.h>
#include "loader.h"

/* Relocations and eglGetProcAddress must resolve exactly the same adapters. */
void* loader_get_bridge_proc_address(const char* name)
{
    if(!name) {
        return NULL;
    }

    size_t left = 0;
    size_t right = sizeof(__bridge_symbols) / sizeof(__bridge_symbols[0]);
    while(left < right) {
        size_t middle = left + (right - left) / 2;
        int result = strcmp(name, __bridge_symbols[middle].symbol_name);
        if(result == 0) {
            return (void*)__bridge_symbols[middle].bridge_proc;
        }

        if(result < 0) {
            right = middle;
        } else {
            left = middle + 1;
        }
    }

    return NULL;
}
