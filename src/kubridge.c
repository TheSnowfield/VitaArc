#include <stdint.h>
#include <string.h>
#include <kubridge.h>
#include "kubridge_ext.h"

int kuKernelCpuUnrestrictedMemset(void* destination, int value, size_t length)
{
    uint8_t fill_buffer[1024];
    memset(fill_buffer, value, sizeof(fill_buffer));

    uint8_t* output = destination;
    while(length > 0) {
        size_t chunk = length < sizeof(fill_buffer) ? length : sizeof(fill_buffer);
        int result = kuKernelCpuUnrestrictedMemcpy(output, fill_buffer, chunk);
        if(result < 0) {
            return result;
        }

        output += chunk;
        length -= chunk;
    }

    return 0;
}
