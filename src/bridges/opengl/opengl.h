#ifndef __BRIDGES_OPENGL_OPENGL_H
#define __BRIDGES_OPENGL_OPENGL_H

#include <dynalib.h>

void bridge_patch_gl(dynalib_t* library);

void bridge_gl_shutdown_buffers(void);

#endif /* __BRIDGES_OPENGL_OPENGL_H */
