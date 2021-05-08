#include <common/define.h>
#include "patcher.h"

void patchSymbols(HSOLIB hSoLibrary, const BRIDGEFUNC bridgeFunc[], uint32_t bridgeSize)
{
  for (int i = 0; i < bridgeSize; ++i)
  {
    logV(TAG, "Patcher processing on 0x%08X %s",
         (uint32_t)bridgeFunc[i].pfnBridgeProc, bridgeFunc[i].szSymbolName);

    // solibGetSymbolStub();
    // solibGetSymbolStub(hSoLibrary, bridgeFunc[i].szSymbolName, bridgeFunc[i].pfnBridgeProc);
  }
}
