#ifndef _LOGCAT_LOGCAT_H_
#define _LOGCAT_LOGCAT_H_

typedef enum LOGLEVEL
{
  VERBOSE = 0,
  INFORMATION,
  WARNING,
  ERROR,
  FATAL
} LOGLEVEL;

void logBegin(const char *logFilePath);

void logEnd();

void logV(const char *tag, const char *format, ...);

void logI(const char *tag, const char *format, ...);

void logW(const char *tag, const char *format, ...);

void logE(const char *tag, const char *format, ...);

void logF(const char *tag, const char *format, ...);

#endif /* _LOGCAT_LOGCAT_H_ */
