#include <stdio.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#include <psp2/power.h>
#include <vitaGL.h>

#include "bridges/jni/jni.h"
#include "bridges/opengl/opengl.h"
#include "bridges/cocos2dx/cocos2dx.h"
#include "bridges/audio/manager.h"

#include "loader.h"
#include "logcat/logcat.h"
#include "config.h"

void setup_performance_profile(void) {
  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);
}

int main(void) {

  // Begin log
  log_begin(PATH_TO_LOGFILE); {

    // Create vita GL
    vglSetupRuntimeShaderCompiler(SHARK_OPT_UNSAFE, SHARK_ENABLE, SHARK_ENABLE, SHARK_ENABLE);
    vglSetVDMBufferSize(128 * 1024);         // default 128 * 1024
    vglSetVertexBufferSize(2 * 1024 * 1024); // default 2 * 1024 * 1024
    vglSetFragmentBufferSize(512 * 1024);    // default 512 * 1024
    vglSetUSSEBufferSize(16 * 1024);         // default 16 * 1024
    vglInitExtended(0, 960, 544, 24 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE); {
      // glClear(GL_COLOR_BUFFER_BIT);
      // glClearColor(1, 1, 1, 0.5);
      // vglSwapBuffers(GL_FALSE);
    }

    // Load library
    dynalib_t *_cocos =
        loader_load_library(LIBRARY_LIBCOCOS2DCPP); {

      // Check success
      if (!_cocos) {
        log_f(TAG, "Load cocos2dx library failed! Exit.");
        goto exit_program;
      }

      // Apply library-specific patches
      bridge_patch_gl(_cocos);
      bridge_patch_cocos2dx(_cocos);
      bridge_audio_provider(_cocos);

      // Initialize library
      loader_init_library(_cocos);
    }

      // Call JNI Main
      bridge_call_jni_main(_cocos);

      // // Init native Cocos2dx
      // bridgeJNICocosInitApp(hLibCocos2dx);
      // bridgeJNICocosSetDeviceId(hLibCocos2dx);
      // bridgeJNICocosNativeInit(hLibCocos2dx, 960, 544);
  }

exit_program:
  for(;;);

  log_end();

  return 0;
}
