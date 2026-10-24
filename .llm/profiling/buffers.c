#include "../../src/bridges/opengl/buffers.c"
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>

void profile_buffers(void)
{
    unsigned count = 0;
    unsigned deleted = 0;
    for(bridge_buffer_t* buffer = __buffers; buffer; buffer = buffer->next) {
        ++count;
        deleted += buffer->deleted;
    }

    uint64_t start = sceKernelGetProcessTimeWide();
    for(unsigned index = 0; index < 128; ++index) {
        __collect_deleted();
    }

    unsigned micros = (unsigned)(sceKernelGetProcessTimeWide() - start);
    sceClibPrintf("PROFILE buffers=%u deleted=%u collect128_us=%u\n", count, deleted, micros);
}
