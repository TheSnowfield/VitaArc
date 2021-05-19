#include <common/define.h>
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
