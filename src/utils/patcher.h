#ifndef _UTILS_PATCHER_H_
#define _UTILS_PATCHER_H_

#include <common/types.h>
#include <solibrary/solib.h>

typedef struct BRIDGEFUNC
{
  const char *szSymbolName;
  uintptr_t *pfnBridgeProc;

} BRIDGEFUNC, *LPBRIDGEFUNC;

typedef struct PATCHADDRESS
{
  uintptr_t nPatchOffset;
  uint64_t nPatchValue;
  uint8_t nPatchLength;
} PATCHADDRESS, *LPPATCHADDRESS;

void patchSymbols(HSOLIB hSoLibrary, const BRIDGEFUNC bridgeFunc[], uint32_t bridgeSize);

void patchAddress(HSOLIB hSoLibrary, const PATCHADDRESS patches[], uint32_t patchSize);

void patchUint32(HSOLIB hSoLibrary, uint32_t nPatchOffset, uint32_t nPatchValue);

void patchUint16(HSOLIB hSoLibrary, uint32_t nPatchOffset, uint16_t nPatchValue);

void patchThumb(HSOLIB hSoLibrary, uint32_t nPatchOffset, uint16_t nPatchValue);

void patchARM(HSOLIB hSoLibrary, uint32_t nPatchOffset, uint32_t nPatchValue);

#endif /* _UTILS_PATCHER_H_ */
