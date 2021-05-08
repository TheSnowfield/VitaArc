#include <logcat/logcat.h>
#include "debug.h"

void debugPrintMemoryBlock(void *lpMemoryBlock, uint32_t nBlockCount)
{
  uint8_t *lpMemory = (uint8_t *)lpMemoryBlock;

  for (int i = 0; i < nBlockCount; ++i)
  {
    for (int j = 0; j < 16; ++j)
    {
      logPrintf("%02X ", *lpMemory);
      ++lpMemory;
    }
    logPrintf("\n");
  }
}
