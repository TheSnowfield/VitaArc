#ifndef __LOGCAT_LOGCAT_H
#define __LOGCAT_LOGCAT_H

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

#define TAG ((__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__))

void log_end(void);

void log_begin(const char *log_file_path);

void log_base(LOGLEVEL level, const char *tag,
              const char *format, va_list args);

void log_printf(const char *format, ...);

void log_v(const char *tag, const char *format, ...);

void log_i(const char *tag, const char *format, ...);

void log_w(const char *tag, const char *format, ...);

void log_e(const char *tag, const char *format, ...);

void log_f(const char *tag, const char *format, ...);

#endif /* __LOGCAT_LOGCAT_H */
