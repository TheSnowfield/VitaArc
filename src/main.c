#include <stdio.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#include <psp2/power.h>
#include <vitaGL.h>

#include "bridges/jni.h"
#include "bridges/libc.h"
#include "bridges/opengl.h"
#include "bridges/cocos2dx.h"
#include "bridges/androidndk.h"
#include "bridges/androidlog.h"
#include "bridges/eabi.h"

#include "solibrary/solib.h"
#include "logcat/logcat.h"
#include "common/define.h"

void setupVitaGL()
{
  // Initial vitagl
  vglSetupRuntimeShaderCompiler(SHARK_OPT_UNSAFE, SHARK_ENABLE, SHARK_ENABLE, SHARK_ENABLE);
  vglSetVDMBufferSize(128 * 1024);         // default 128 * 1024
  vglSetVertexBufferSize(2 * 1024 * 1024); // default 2 * 1024 * 1024
  vglSetFragmentBufferSize(512 * 1024);    // default 512 * 1024
  vglSetUSSEBufferSize(16 * 1024);         // default 16 * 1024
  vglSetVertexPoolSize(48 * 1024 * 1024);
  vglInitExtended(0, 960, 544, 24 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE);
  vglUseVram(GL_TRUE);
}

void setupPerformanceProfile()
{
  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);
}

int main()
{
  // Setup environment
  setupPerformanceProfile();
  setupVitaGL();

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
    bridgePatchJNI(hLibCocos2dx);
    bridgePatchEABI(hLibCocos2dx);
    bridgePatchAndroidNDK(hLibCocos2dx);
    bridgePatchAndroidLog(hLibCocos2dx);
    bridgePatchGL(hLibCocos2dx);
    bridgePatchCocos2DX(hLibCocos2dx);

    // Call JNI Main
    bridgeCallJNIMain(hLibCocos2dx);
  }

ExitProgram:
  logEnd();

  return 0;
}
