#include <stdarg.h>
#include <common/define.h>
#include <utils/patcher.h>
#include <logcat/logcat.h>

#include "androidlog.h"

int __android_log_print(int prio, const char *tag, const char *format, ...)
{
  VARG_WRAP(logBase((LOGLEVEL)prio, tag, format, opt));
  return 0;
}

int __android_log_write(int prio, const char *tag, const char *format, ...)
{
  VARG_WRAP(logBase((LOGLEVEL)prio, tag, format, opt));
  return 0;
}

static const BRIDGEFUNC BRIDGE_ANDROIDLOG[] =
    {
        {"__android_log_print", (uintptr_t)&__android_log_print},
        {"__android_log_write", (uintptr_t)&__android_log_write},
};

void bridgePatchAndroidLog(HSOLIB hSoLibrary)
{
  patchSymbols(hSoLibrary, BRIDGE_ANDROIDLOG,
               sizeof(BRIDGE_ANDROIDLOG) / sizeof(BRIDGEFUNC));
}
