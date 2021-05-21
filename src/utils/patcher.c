#include <common/define.h>
#include <logcat/logcat.h>
#include <kubridge/kubridge.h>
#include "kubridge.h"
#include "patcher.h"

void patchSymbols(HSOLIB hSoLibrary, const BRIDGEFUNC bridgeFunc[], uint32_t bridgeSize)
{
  for (int i = 0; i < bridgeSize; ++i)
  {
    // Install proc
    solibInstallProc(hSoLibrary,
                     bridgeFunc[i].szSymbolName,
                     bridgeFunc[i].pfnBridgeProc);
  }
}

void patchAddress(HSOLIB hSoLibrary, const PATCHADDRESS patches[], uint32_t patchSize)
{
  // Get image base
  uintptr_t lpImageBase = 0x98000000;
  {
    if (!lpImageBase)
    {
      logE(TAG, "Patch image base is NULL");
      return;
    }
  }

  // Apply patches
  for (int i = 0; i < patchSize; ++i)
  {
    // Calculate address
    uintptr_t lpPatchAddress = lpImageBase + patches[i].nPatchOffset;

    // Apply patch
    if (patches[i].nPatchLength < 8)
      kuKernelCpuUnrestrictedMemcpy(lpPatchAddress,
                                    &patches[i].nPatchValue,
                                    patches[i].nPatchLength);
    else
      logW(TAG, "Patch length over 8 bytes.");
  }
}
