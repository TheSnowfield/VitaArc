#include <utils/patcher.h>
#include <utils/debug.h>
#include <stdlib.h>
#include "cocos2dx.h"

void bridgePatchCocos2DX(HSOLIB hSoLibrary)
{
  void *lpPatchAddress = NULL;

  // cocos2d::experimental::FrameBuffer::_frameBuffers
  // read nullptr
  // lpPatchAddress = malloc(4);
  // patchUint32(hSoLibrary, 0xA5F750, lpPatchAddress);

  // cocos2d::JniHelper::deleteLocalRefs [BLX LR]
  // __next_prime underflow exception in unordered_map
  // patchARM(hSoLibrary, 0x393B68, 0xE12FFF3E);

  // cocos2d::JniHelper::convert
  // unordered_map crashed
  // patchThumb(hSoLibrary, 0x5C2848 + 0, 0x00BF);
  // patchThumb(hSoLibrary, 0x5C2848 + 2, 0x00BF);
  // patchThumb(hSoLibrary, 0x5C284C + 0, 0x00BF);
  // patchThumb(hSoLibrary, 0x5C284E + 0, 0x00BF);
  // patchThumb(hSoLibrary, 0x5C2850 + 0, 0x00BF);
  // patchThumb(hSoLibrary, 0x5C2850 + 2, 0x00BF);

  // AudioManager::init
  // prevent initialize opensles
  // patchThumb(hSoLibrary, 0x50E284, 0x00BF);
}
