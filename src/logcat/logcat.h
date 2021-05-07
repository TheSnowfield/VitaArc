#ifndef _LOGCAT_LOGCAT_H_
#define _LOGCAT_LOGCAT_H_

#include <stdarg.h>

typedef enum LOGLEVEL
{
  VERBOSE = 0,
  INFORMATION,
  WARNING,
  ERROR,
  FATAL
} LOGLEVEL;

#define VARG_WRAP(x)     \
  va_list opt;           \
  va_start(opt, format); \
  x;                     \
  va_end(opt);

void logEnd();

void logBegin(const char *logFilePath);

void logBase(LOGLEVEL level, const char *tag, const char *format, va_list args);

void logV(const char *tag, const char *format, ...);

void logI(const char *tag, const char *format, ...);

void logW(const char *tag, const char *format, ...);

void logE(const char *tag, const char *format, ...);

void logF(const char *tag, const char *format, ...);

#endif /* _LOGCAT_LOGCAT_H_ */
