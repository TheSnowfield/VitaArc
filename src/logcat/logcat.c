#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <psp2/io/fcntl.h>

#include <common/types.h>
#include <common/defines.h>
#include "logcat.h"

SceUID logFile;
bool logStarted = false;
const char logLvString[] = {'V', 'I', 'W', 'E', 'F'};

void logBegin(const char *logFilePath)
{
  if (logStarted)
  {
    logW(TAG, "Calling logBegin() while logging.");
    return;
  }

  // Open log file
  logFile = sceIoOpen(logFilePath, SCE_O_CREAT | SCE_O_WRONLY, 0777);
  if (logFile <= 0)
    return;

  logStarted = true;
  logI(TAG, "Log started.");
}

void logEnd()
{
  if (!logStarted)
    return;

  // Stop log
  logStarted = false;
  logI(TAG, "Log stop.");

  // Close file
  sceIoClose(logFile);
}

void logBase(LOGLEVEL level, const char *tag, const char *format, ...)
{
  char logBuffer[1024] = {0x00};
  char *logPosition = logBuffer;
  va_list opt;

  va_start(opt, format);
  {
    logPosition += snprintf(logPosition, sizeof(logBuffer), "[%c] [%s] \t", logLvString[level], tag);
    logPosition += vsnprintf(logPosition, sizeof(logBuffer), format, opt);
    logPosition += snprintf(logPosition, sizeof(logBuffer), "\n");
    sceIoWrite(logFile, logBuffer, logPosition - logBuffer);
  }
  va_end(opt);
}

void inline logV(const char *tag, const char *format, ...)
{
  logBase(VERBOSE, tag, format);
}

void inline logI(const char *tag, const char *format, ...)
{
  logBase(INFORMATION, tag, format);
}

void inline logW(const char *tag, const char *format, ...)
{
  logBase(WARNING, tag, format);
}

void inline logE(const char *tag, const char *format, ...)
{
  logBase(ERROR, tag, format);
}

void inline logF(const char *tag, const char *format, ...)
{
  logBase(FATAL, tag, format);
}
