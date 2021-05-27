#include "../../../logcat/logcat.h"
#include "log.h"

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
