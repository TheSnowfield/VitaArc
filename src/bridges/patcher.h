#ifndef _UTILS_PATCHER_H_
#define _UTILS_PATCHER_H_

#include "types.h"
#include "loader.h"


typedef struct BRIDGEFUNC
{
  const char *szSymbolName;
  uintptr_t pfnBridgeProc;

} BRIDGEFUNC, *LPBRIDGEFUNC;

typedef struct PATCHADDRESS
{
  uintptr_t nPatchOffset;
  uint64_t nPatchValue;
  uint8_t nPatchLength;
} PATCHADDRESS, *LPPATCHADDRESS;

void patchSymbols(dynalib_t *hSoLibrary, const BRIDGEFUNC bridgeFunc[], uint32_t bridgeSize);

void patchAddress(dynalib_t *hSoLibrary, const PATCHADDRESS patches[], uint32_t patchSize);

void patchUint32(dynalib_t *hSoLibrary, uint32_t nPatchOffset, uint32_t nPatchValue);

void patchUint16(dynalib_t *hSoLibrary, uint32_t nPatchOffset, uint16_t nPatchValue);

void patchThumb(dynalib_t *hSoLibrary, uint32_t nPatchOffset, uint16_t nPatchValue);

void patchARM(dynalib_t *hSoLibrary, uint32_t nPatchOffset, uint32_t nPatchValue);

void hookStubProc(dynalib_t *hSoLibrary, uint32_t nHookOffset, void *lpfnHookCallback);

#endif /* _UTILS_PATCHER_H_ */
