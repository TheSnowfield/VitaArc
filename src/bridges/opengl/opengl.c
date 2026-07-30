#include <config.h>
#include <patcher.h>
#include <logcat/logcat.h>

#include "opengl.h"
#include "impl/opengl.h"

void bridge_patch_gl(dynalib_t *library)
{
  (void)library;
  // cocos2d::GLProgram::bindPredefinedVertexAttribs
  // calling unsupported function
  // patch_arm(hSoLibrary, 0x6E6AD4, 0x00BF00BF);

  // cocos2d::GLProgram::link
  // calling unsupported function
  // patch_arm(hSoLibrary, 0x6E6534, 0x00BF00BF);

}
