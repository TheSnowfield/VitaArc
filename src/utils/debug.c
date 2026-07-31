#include <psp2/io/fcntl.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <kubridge.h>
#include <logcat/logcat.h>
#include <config.h>
#include <vitaGL.h>
#include "debug.h"

static bool __save_pixels(const char* path, const unsigned char* pixels, size_t size)
{
    SceUID file = sceIoOpen(path, SCE_O_CREAT | SCE_O_WRONLY | SCE_O_TRUNC, 0666);
    bool saved = file >= 0;
    size_t written = 0;
    while(saved && written < size) {
        int result = sceIoWrite(file, pixels + written, size - written);
        if(result <= 0) {
            saved = false;
        } else {
            written += (size_t)result;
        }
    }

    if(file >= 0 && sceIoClose(file) < 0) {
        saved = false;
    }

    return saved;
}

bool debug_capture_frame(const char* path, uint32_t width, uint32_t height)
{
    if(!path || !width || !height || width > 4096 || height > 4096) {
        return false;
    }

    size_t size = (size_t)width * height * 4;
    unsigned char* pixels = vglMalloc(size);
    if(!pixels) {
        return false;
    }

    glFinish();
    vglReadPixels(0, 0, (GLsizei)width, (GLsizei)height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    bool saved = __save_pixels(path, pixels, size);
    vglFree(pixels);
    return saved;
}

bool debug_capture_display_frame(const char* path)
{
    SceDisplayFrameBuf frame = {.size = sizeof(frame)};

    glFinish();
    if(!path || sceGxmDisplayQueueFinish() < 0 || sceDisplayGetFrameBuf(&frame, SCE_DISPLAY_SETBUF_IMMEDIATE) < 0 ||
        !frame.base || !frame.width || !frame.height || frame.width > 4096 || frame.height > 4096 ||
        frame.pitch < frame.width || frame.pitch > 4096 || frame.pixelformat != SCE_DISPLAY_PIXELFORMAT_A8B8G8R8) {
        return false;
    }

    log_i(TAG,
        "Displayed framebuffer: %ux%u pitch=%u format=%u base=%p",
        frame.width,
        frame.height,
        frame.pitch,
        frame.pixelformat,
        frame.base);
    size_t size = (size_t)frame.width * frame.height * 4;
    unsigned char* pixels = vglMalloc(size);
    if(!pixels) {
        return false;
    }

    // Read the actual display surface through GXM, independently of vitaGL's
    // active read framebuffer and the emulator's CPU surface cache.
    int result = sceGxmTransferCopy(frame.width,
        frame.height,
        0,
        0,
        SCE_GXM_TRANSFER_COLORKEY_NONE,
        SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR,
        SCE_GXM_TRANSFER_LINEAR,
        frame.base,
        0,
        0,
        frame.pitch * 4,
        SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR,
        SCE_GXM_TRANSFER_LINEAR,
        pixels,
        0,
        0,
        frame.width * 4,
        NULL,
        0,
        NULL);
    bool saved = result >= 0 && sceGxmTransferFinish() >= 0 && __save_pixels(path, pixels, size);
    vglFree(pixels);
    return saved;
}

void debug_print_memory_block(void* memory_block, uint32_t block_count, uint8_t align_size)
{
    uint8_t* memory = (uint8_t*)memory_block;
    align_size = align_size == 0 ? 16 : align_size;

    for(int i = 0, j = 0; i < block_count * align_size; ++i) {
        log_printf("%02X ", *memory);

        if(++j >= 16) {
            log_printf("\n"), j = 0;
        }

        ++memory;
    }
}

// Dump data in the kernel
// to bypass access permissions
// #define MEMDUMP_IN_KERNEL

void debug_memory_dump(const char* file_path, void* memory_block, uint32_t size)
{
    SceUID file_id = sceIoOpen(file_path, SCE_O_CREAT | SCE_O_WRONLY, 0777);

#ifdef MEMDUMP_IN_KERNEL
    void* memory_block_base = NULL;

    SceKernelAllocMemBlockKernelOpt allocation_options = {0};

    allocation_options.size = sizeof(SceKernelAllocMemBlockKernelOpt);

    SceUID memory_block_id =
        kuKernelAllocMemBlock("girl_blessing", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 0x1000, &allocation_options);
    {
        if(memory_block_id <= 0) {
            log_w(TAG, "Alloc memory failed.");
            return;
        }

        sceKernelGetMemBlockBase(memory_block_id, &memory_block_base);

        if(memory_block_base <= 0) {
            sceKernelFreeMemBlock(memory_block_id);

            log_w(TAG, "Get memblock base failed.");
            return;
        }
    }

    // write several block
    uint32_t block_count = size / 0x1000;
    uint32_t remaining_length = size - block_count * 0x1000;
    for(int i = 0; i < block_count; ++i, memory_block += 0x1000) {
        kuKernelCpuUnrestrictedMemcpy(memory_block_base, memory_block, 0x1000);
        sceIoWrite(file_id, memory_block_base, 0x1000);
    }

    // ends up
    kuKernelCpuUnrestrictedMemcpy(memory_block_base, memory_block, remaining_length);
    sceIoWrite(file_id, memory_block_base, remaining_length);

    sceKernelFreeMemBlock(memory_block_id);

#else

    sceIoWrite(file_id, memory_block, size);

#endif

    sceIoClose(file_id);
}

void debug_break_point(void* memory_address)
{
    // mov r0, #0; ldr r0, [r0];
    const char dead_code[] = {0x4F, 0xF0, 0x00, 0x00, 0x00, 0x68};

    kuKernelCpuUnrestrictedMemcpy(memory_address, &dead_code, sizeof(dead_code));
}
