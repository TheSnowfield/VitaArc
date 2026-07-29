#ifndef _BRIDGE_COCOS2DX_H_
#define _BRIDGE_COCOS2DX_H_

#include <loader.h>

void bridgePatchCocos2DX(dynalib_t *hSoLibrary);

void bridgeCocos2DXRendererNativeInit(dynalib_t *hSoLibrary,
                                    uint32_t nScreenWidth, uint32_t nScreenHeight);

#endif /* _BRIDGE_COCOS2DX_H_ */
