#ifndef __BRIDGES_COCOS2DX_COCOS2DX_H
#define __BRIDGES_COCOS2DX_COCOS2DX_H

#include <dynalib.h>

void bridge_patch_cocos2dx(dynalib_t *library);

void bridge_cocos2dx_renderer_native_init(dynalib_t *library,
                                          uint32_t screen_width,
                                          uint32_t screen_height);

#endif /* __BRIDGES_COCOS2DX_COCOS2DX_H */
