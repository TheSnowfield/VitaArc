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
#include "bridges/libc/libc.h"
#include "bridges/opengl/opengl.h"
#include "bridges/cocos2dx/cocos2dx.h"
#include "bridges/android/android.h"
#include "bridges/eabi/eabi.h"

#include "solibrary/solib.h"
#include "logcat/logcat.h"
#include "common/define.h"

void setupPerformanceProfile()
{
  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);
}

int main()
{
  // Begin log
  logBegin(PATH_TO_LOGFILE);
  {
    // Load library
    HSOLIB hLibCocos2dx = solibLoadLibrary(LIBRARY_LIBCOCOS2DCPP);
    // HSOLIB hLibCrashlytics = solibLoadLibrary(LIBRARY_LIBCRASHLYTICS);
    // HSOLIB hLibCrashlyticsCommon = solibLoadLibrary(LIBRARY_LIBCRASHLYTICS_COMMON);
    // HSOLIB hLibCrashlyticsHandler = solibLoadLibrary(LIBRARY_LIBCRASHLYTICS_HANDLER);
    // HSOLIB hLibCrashlyticsTrampoline =77 solibLoadLibrary(LIBRARY_LIBCRASHLYTICS_TRAMPOLINE);
    // HSOLIB hLibFMOD = solibLoadLibrary(LIBRARY_LIBFMOD);
    // HSOLIB hLibFMODProvider = solibLoadLibrary(LIBRARY_LIBFMOD_PROVIDER);

    // Check success
    if (!hLibCocos2dx)
    {
      logF(TAG, "Load cocos2dx library failed! Exit.");
      goto ExitProgram;
    }

    // Setup bridges
    bridgePatchEABI(hLibCocos2dx);
    bridgePatchAndroid(hLibCocos2dx);
    bridgePatchLibC(hLibCocos2dx);
    bridgePatchGL(hLibCocos2dx);
    bridgePatchCocos2DX(hLibCocos2dx);
    bridgeAudioProvider(hLibCocos2dx);
    solibInitLibrary(hLibCocos2dx);

    // Call JNI Main
    bridgeCallJNIMain(hLibCocos2dx);

    // Initial vitagl
    vglSetupRuntimeShaderCompiler(SHARK_OPT_UNSAFE, SHARK_ENABLE, SHARK_ENABLE, SHARK_ENABLE);
    vglSetVDMBufferSize(128 * 1024);         // default 128 * 1024
    vglSetVertexBufferSize(2 * 1024 * 1024); // default 2 * 1024 * 1024
    vglSetFragmentBufferSize(512 * 1024);    // default 512 * 1024
    vglSetUSSEBufferSize(16 * 1024);         // default 16 * 1024
    vglSetVertexPoolSize(48 * 1024 * 1024);
    vglInitExtended(0, 960, 544, 24 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE);
    vglUseVram(GL_TRUE);
    {
      // glClear(GL_COLOR_BUFFER_BIT);
      // glClearColor(1, 1, 1, 0.5);
      // vglSwapBuffers(GL_FALSE);

      // Init Cocos2dx
      bridgeJNICocosInitApp(hLibCocos2dx);
      bridgeJNICocosSetDeviceId(hLibCocos2dx);
      bridgeJNICocosNativeInit(hLibCocos2dx, 960, 544);
    }
  }

ExitProgram:
  logEnd();

  return 0;
}
