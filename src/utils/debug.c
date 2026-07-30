#include <psp2/io/fcntl.h>
#include <kubridge/kubridge.h>
#include <logcat/logcat.h>
#include <config.h>
#include "debug.h"

void debug_print_memory_block(void *memory_block,
                              uint32_t block_count, uint8_t align_size)
{
  uint8_t *memory = (uint8_t *)memory_block;
  align_size = align_size == 0 ? 16 : align_size;

  for (int i = 0, j = 0; i < block_count * align_size; ++i)
  {
    log_printf("%02X ", *memory);

    if (++j >= 16)
      log_printf("\n"), j = 0;

    ++memory;
  }
}

// Dump data in the kernel
// to bypass access permissions
// #define MEMDUMP_IN_KERNEL

void debug_memory_dump(const char *file_path, void *memory_block, uint32_t size)
{
  SceUID file_id = sceIoOpen(file_path, SCE_O_CREAT | SCE_O_WRONLY, 0777);

#ifdef MEMDUMP_IN_KERNEL
  void *memory_block_base = NULL;

  SceKernelAllocMemBlockKernelOpt allocation_options = {0};
  allocation_options.size = sizeof(SceKernelAllocMemBlockKernelOpt);

  SceUID memory_block_id =
      kuKernelAllocMemBlock("girl_blessing",
                            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
                            0x1000, &allocation_options);
  {
    if (memory_block_id <= 0)
    {
      log_w(TAG, "Alloc memory failed.");
      return;
    }

    sceKernelGetMemBlockBase(memory_block_id, &memory_block_base);

    if (memory_block_base <= 0)
    {
      sceKernelFreeMemBlock(memory_block_id);

      log_w(TAG, "Get memblock base failed.");
      return;
    }
  }

  // write several block
  uint32_t block_count = size / 0x1000;
  uint32_t remaining_length = size - block_count * 0x1000;
  for (int i = 0; i < block_count; ++i, memory_block += 0x1000)
  {
    kuKernelCpuUnrestrictedMemcpy(memory_block_base, memory_block, 0x1000);
    sceIoWrite(file_id, memory_block_base, 0x1000);
  }

  // ends up
  kuKernelCpuUnrestrictedMemcpy(memory_block_base, memory_block,
                                remaining_length);
  sceIoWrite(file_id, memory_block_base, remaining_length);

  sceKernelFreeMemBlock(memory_block_id);

#else

  sceIoWrite(file_id, memory_block, size);

#endif

  sceIoClose(file_id);
}

void debug_break_point(void *memory_address)
{
  // mov r0, #0; ldr r0, [r0];
  const char dead_code[] = {0x4F, 0xF0, 0x00, 0x00, 0x00, 0x68};
  kuKernelCpuUnrestrictedMemcpy(memory_address, &dead_code, sizeof(dead_code));
}
