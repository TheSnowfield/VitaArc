#include <utils/patcher.h>
#include "cocos2dx.h"
#include <stdlib.h>

typedef int (*Cocos2dxRendererNativeInit)(uint32_t, uint32_t);

void bridgeCocos2DXRendererNativeInit(HSOLIB hSoLibrary,
                                      uint32_t nScreenWidth, uint32_t nScreenHeight)
{
  Cocos2dxRendererNativeInit lpfnNativeInit =
      solibGetProcAddress(hSoLibrary, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit");

  lpfnNativeInit(nScreenWidth, nScreenHeight);
}

void bridgePatchCocos2DX(HSOLIB hSoLibrary)
{
  // cocos2d::experimental::FrameBuffer::_frameBuffers
  void *lpPatchAddress = malloc(4);
  patchUint32(hSoLibrary, 0xA5F750, lpPatchAddress);
}
