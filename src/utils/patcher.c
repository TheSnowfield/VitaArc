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

void patchUint32(HSOLIB hSoLibrary, uint32_t nPatchOffset, uint32_t nPatchValue)
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

  // Calculate address
  uintptr_t lpPatchAddress = lpImageBase + nPatchOffset;

  // Apply patch
  kuKernelCpuUnrestrictedMemcpy(lpPatchAddress,
                                &nPatchValue, 4);
}

void patchUint16(HSOLIB hSoLibrary, uint32_t nPatchOffset, uint16_t nPatchValue)
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

  // Calculate address
  uintptr_t lpPatchAddress = lpImageBase + nPatchOffset;

  // Apply patch
  kuKernelCpuUnrestrictedMemcpy(lpPatchAddress,
                                &nPatchValue, 2);
}

void patchThumb(HSOLIB hSoLibrary, uint32_t nPatchOffset, uint16_t nPatchValue)
{
  return patchUint16(hSoLibrary, nPatchOffset, nPatchValue);
}

void patchARM(HSOLIB hSoLibrary, uint32_t nPatchOffset, uint32_t nPatchValue)
{
  return patchUint32(hSoLibrary, nPatchOffset, nPatchValue);
}

void *hookStubProc(HSOLIB hSoLibrary, uint32_t nHookOffset, void *lpfnHookCallback)
{
  if (!hSoLibrary ||
      !nHookOffset ||
      !lpfnHookCallback)
    return NULL;

  // Get image base
  uintptr_t lpImageBase = 0x98000000;
  {
    if (!lpImageBase)
    {
      logE(TAG, "Patch image base is NULL");
      return;
    }
  }

  // Calculate address
  uintptr_t lpHookAddress = (lpImageBase + nHookOffset);

  // hook
  uint32_t nHook[2];
  nHook[0] = 0xE51FF004; // LDR PC, [PC, #-4]
  nHook[1] = lpfnHookCallback;
  kuKernelCpuUnrestrictedMemcpy((void *)lpHookAddress, nHook, sizeof(nHook));
}
