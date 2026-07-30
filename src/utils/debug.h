#ifndef __UTILS_DEBUG_H
#define __UTILS_DEBUG_H

#define BREAKPOINT(x) \
  static int __trigger_count = 0;  \
  if (++__trigger_count == x)      \
  asm volatile("mov r0, #0; ldr r0, [r0];")

#include <stdint.h>

void debug_print_memory_block(void *memory_block,
                              uint32_t block_count, uint8_t align_size);

void debug_memory_dump(const char *file_path, void *memory_block, uint32_t size);

void debug_break_point(void *memory_address);

#endif /* __UTILS_DEBUG_H */
