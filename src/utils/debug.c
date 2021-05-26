#include <psp2/io/fcntl.h>
#include <kubridge/kubridge.h>
#include <logcat/logcat.h>
#include <common/define.h>
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

// Dump data in the kernel
// to bypass access permissions
// #define MEMDUMP_IN_KERNEL

void debugMemoryDump(const char *szFile, void *lpMemoryBlock, uint32_t size)
{
  SceUID nFileId = sceIoOpen(szFile, SCE_O_CREAT | SCE_O_WRONLY, 0777);

#ifdef MEMDUMP_IN_KERNEL
  void *lpMemblock = NULL;

  SceKernelAllocMemBlockKernelOpt sAllocOption = {0};
  sAllocOption.size = sizeof(SceKernelAllocMemBlockKernelOpt);

  SceUID nMemblockId = kuKernelAllocMemBlock("girl_blessing",
                                             SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
                                             0x1000, &sAllocOption);
  {
    if (nMemblockId <= 0)
    {
      logW(TAG, "Alloc memory failed.");
      return;
    }

    sceKernelGetMemBlockBase(nMemblockId, &lpMemblock);

    if (lpMemblock <= 0)
    {
      sceKernelFreeMemBlock(nMemblockId);

      logW(TAG, "Get memblock base failed.");
      return;
    }
  }

  // write several block
  uint32_t nBlocks = size / 0x1000;
  uint32_t nRemainLen = size - nBlocks * 0x1000;
  for (int i = 0; i < nBlocks; ++i, lpMemoryBlock += 0x1000)
  {
    kuKernelCpuUnrestrictedMemcpy(lpMemblock, lpMemoryBlock, 0x1000);
    sceIoWrite(nFileId, lpMemblock, 0x1000);
  }

  // ends up
  kuKernelCpuUnrestrictedMemcpy(lpMemblock, lpMemoryBlock, nRemainLen);
  sceIoWrite(nFileId, lpMemblock, nRemainLen);

  sceKernelFreeMemBlock(nMemblockId);

#else

  sceIoWrite(nFileId, lpMemoryBlock, size);

#endif

  sceIoClose(nFileId);
}

void debugBreakPoint(void *lpMemoryAddress)
{
  // LDR R0, #0
  static uint8_t nDeadCode[] = {0x48, 0x00};
  kuKernelCpuUnrestrictedMemcpy(lpMemoryAddress, &nDeadCode, 2);
}
