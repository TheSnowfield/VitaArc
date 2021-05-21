#include <utils/patcher.h>
#include "cocos2dx.h"

typedef int (*Cocos2dxRendererNativeInit)(uint32_t, uint32_t);

static const PATCHADDRESS PATCH_COCOS_STACKCHK[] =
{
  // cocos2d::JniHelper::cacheEnv
  {0x3DF276, 0xBF00, 2}, // LDR R3, [R6] => NOP  // nop stack check head
  {0x3DF2E4, 0xBF00, 2}, // LDR R3, [R6] => NOP  // nop stack check end
  {0x3DF2E6, 0xBF00, 2}, // CMP R2, R3   => NOP  // nop stack check end

  // Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit
  {0x3DE5D6, 0xBF00, 2}, // LDR R3, [R4] => NOP  // nop stack check head
  {0x3DE67E, 0xBF00, 2}, // LDR R3, [R7] => NOP  // nop stack check end
  {0x3DE680, 0xBF00, 2}, // CMP R2, R3   => NOP  // nop stack check end

  // cocos2d::Director::Director
  {0x4D27AC, 0xBF00, 2}, // LDR R3, [R4] => NOP  // nop stack check head
  {0x4D286E, 0xBF00, 2}, // LDR R3, [R7] => NOP  // nop stack check end
  {0x4D2874, 0xBF00, 2}, // CMP R2, R3   => NOP  // nop stack check end
  {0x4D2876, 0xE01C, 2}, // BEQ #0x3c    => B #0x3c  // jump over the check

  // cocos2d::Director::init
  {0x4D2414, 0xBF00, 2},      // LDR R3, [R6]   => NOP  // nop stack check head
  {0x4D2682, 0xE320F000, 4},  // LDR.W R3, [R8] => NOP  // nop stack check end
  {0x4D2688, 0xBF00, 2},      // CMP R2, R3     => NOP  // nop stack check end
  {0x4D268A, 0xE019, 2},      // BEQ #0x36      => B #0x36  // jump over the check

  // cocos2d::Director::setDefaultValues
  {0x4D1006, 0xBF00BF00, 4},  // LDR.W R3, [R9] => NOP  // nop stack check head
  {0x4D11B0, 0xBF00BF00, 4},  // LDR.W R3, [R9] => NOP  // nop stack check end
  {0x4D11B4, 0xBF00, 2},      // CMP R2, R3     => NOP  // nop stack check end
  {0x4D11B6, 0xE022, 2},      // BEQ #0x48      => B #0x48  // jump over the check

  // cocos2d::Configuration::Configuration
  {0x4CBAA4, 0xBF00, 2},  // LDR R3, [R5]   => NOP  // nop stack check head
  {0x4CBB6A, 0xBF00, 2},  // LDR R3, [R7]   => NOP  // nop stack check end
  {0x4CBB6C, 0xBF00, 2},  // CMP R2, R3     => NOP  // nop stack check end
  {0x4CBB70, 0xE017, 2},  // BEQ #0x32      => B #0x32  // jump over the check

  // cocos2d::Configuration::init
  {0x4CBC8C, 0xBF00, 2},      // LDR R3, [R5]   => NOP  // nop stack check head
  {0x4CBD5E, 0xBF00BF00, 4},  // LDR.W R3, [R7] => NOP  // nop stack check end
  {0x4CBD64, 0xBF00, 2},      // CMP R2, R3     => NOP  // nop stack check end
  {0x4CBD66, 0xE010, 2},      // BEQ #0x24      => B #0x24  // jump over the check
};

void bridgeCocos2DXRendererNativeInit(HSOLIB hSoLibrary,
                                    uint32_t nScreenWidth, uint32_t nScreenHeight)
{
  Cocos2dxRendererNativeInit lpfnNativeInit =
      solibGetProcAddress(hSoLibrary, "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit");

  lpfnNativeInit(nScreenWidth, nScreenHeight);
}

void bridgePatchCocos2DX(HSOLIB hSoLibrary)
{
  patchAddress(hSoLibrary, PATCH_COCOS_STACKCHK, sizeof(PATCH_COCOS_STACKCHK) / sizeof(PATCHADDRESS));
}