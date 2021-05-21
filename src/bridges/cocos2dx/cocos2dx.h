#ifndef _BRIDGE_COCOS2DX_H_
#define _BRIDGE_COCOS2DX_H_

#include <solibrary/solib.h>

void bridgePatchCocos2DX(HSOLIB hSoLibrary);

void bridgeCocos2DXRendererNativeInit(HSOLIB hSoLibrary,
                                    uint32_t nScreenWidth, uint32_t nScreenHeight);

#endif /* _BRIDGE_COCOS2DX_H_ */