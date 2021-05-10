#include <psp2/io/fcntl.h>
#include <logcat/logcat.h>
#include "debug.h"

void debugPrintMemoryBlock(void *lpMemoryBlock,
                           uint32_t nBlockCount, uint8_t alignSize)
{
  uint8_t *lpMemory = (uint8_t *)lpMemoryBlock;
  alignSize = alignSize == 0 ? 16 : alignSize;

  for (int i = 0, j = 0; i < nBlockCount * alignSize; ++i)
  {
    logPrintf("%02X ", *lpMemory);

    if (++j >= 16)
      logPrintf("\n"), j = 0;

    ++lpMemory;
  }
}
void debugMemoryDump(void *lpMemoryBlock, uint32_t size)
{
  SceUID nFileId = sceIoOpen("ux0:vitaarc/memdump.bin",
                             SCE_O_CREAT | SCE_O_WRONLY, 0777);
  sceIoWrite(nFileId, lpMemoryBlock, size);
  sceIoClose(nFileId);
}