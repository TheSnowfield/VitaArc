#include <stddef.h>

/* VitaSDK newlib forwards these two operations to Vita. Pure byte operations
 * keep this instruction-level fixture independent of platform initialization. */
void* sceClibMemcpy(void* destination, const void* source, size_t size)
{
    volatile unsigned char* output = destination;
    const volatile unsigned char* input = source;
    while(size--) {
        *output++ = *input++;
    }

    return destination;
}

void* sceClibMemset(void* destination, int value, size_t size)
{
    volatile unsigned char* output = destination;
    while(size--) {
        *output++ = (unsigned char)value;
    }

    return destination;
}
