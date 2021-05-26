#ifndef _UTILS_DEBUG_H_
#define _UTILS_DEBUG_H_

#include <stdint.h>

void debugPrintMemoryBlock(void *lpMemoryBlock,
                           uint32_t nBlockCount, uint8_t alignSize);

void debugMemoryDump(const char* szFile, void *lpMemoryBlock, uint32_t size);

void debugBreakPoint(void *lpMemoryAddress);

#endif /* _UTILS_DEBUG_H_ */
