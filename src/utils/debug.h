#ifndef _UTILS_DEBUG_H_
#define _UTILS_DEBUG_H_

#define BREAKPOINT(x) \
  static int _trigger_count = 0;   \
  if (++_trigger_count == x)       \
  asm volatile("mov r0, #0; ldr r0, [r0];")

#include <stdint.h>

void debugPrintMemoryBlock(void *lpMemoryBlock,
                           uint32_t nBlockCount, uint8_t alignSize);

void debugMemoryDump(const char* szFile, void *lpMemoryBlock, uint32_t size);

void debugBreakPoint(void *lpMemoryAddress);

#endif /* _UTILS_DEBUG_H_ */
