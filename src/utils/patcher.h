#ifndef _UTILS_PATCHER_H_
#define _UTILS_PATCHER_H_

#include <common/types.h>
#include <solibrary/solib.h>

typedef struct BRIDGEFUNC
{
  const char *szSymbolName;
  uintptr_t *pfnBridgeProc;

} BRIDGEFUNC, *LPBRIDGEFUNC;

void patchSymbols(HSOLIB hSoLibrary, const BRIDGEFUNC bridgeFunc[], uint32_t bridgeSize);

#endif /* _UTILS_PATCHER_H_ */
