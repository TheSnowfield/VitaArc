#include <patcher.h>
#include <utils/debug.h>
#include <stdlib.h>
#include "cocos2dx.h"

void bridge_patch_cocos2dx(dynalib_t *library)
{
  // cocos2d::experimental::FrameBuffer::_frameBuffers
  // read nullptr
  // lpPatchAddress = malloc(4);
  // patch_uint32(hSoLibrary, 0xA5F750, lpPatchAddress);

  // cocos2d::JniHelper::deleteLocalRefs [BLX LR]
  // __next_prime underflow exception in unordered_map
  // patch_arm(hSoLibrary, 0x393B68, 0xE12FFF3E);

  // cocos2d::JniHelper::convert
  // unordered_map crashed
  // patch_thumb(hSoLibrary, 0x5C2848 + 0, 0x00BF);
  // patch_thumb(hSoLibrary, 0x5C2848 + 2, 0x00BF);
  // patch_thumb(hSoLibrary, 0x5C284C + 0, 0x00BF);
  // patch_thumb(hSoLibrary, 0x5C284E + 0, 0x00BF);
  // patch_thumb(hSoLibrary, 0x5C2850 + 0, 0x00BF);
  // patch_thumb(hSoLibrary, 0x5C2850 + 2, 0x00BF);

  // AudioManager::init
  // prevent initialize opensles
  // patch_thumb(hSoLibrary, 0x50E284, 0x00BF);

  // cocos2d::Node::onEnter
  // read nullptr
  patch_thumb(library, 0x69A342, 0x00BF);

  // AudioManager::internal_loadSFX
  // prevent call opensles
  patch_thumb(library, 0x50E30E, 0x00BF);

  // cocos2d::GLProgram::bindPredefinedVertexAttribs
  // prevent call glBindAttribLocation
  patch_arm(library, 0x6E6AD4, 0x00F020E3);

}
