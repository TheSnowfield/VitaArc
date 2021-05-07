#ifndef _BRIDGE_ANDROIDLOG_IMPL_H_
#define _BRIDGE_ANDROIDLOG_IMPL_H_

#include <logcat/logcat.h>

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

#endif /* _BRIDGE_ANDROIDLOG_IMPL_H_ */
