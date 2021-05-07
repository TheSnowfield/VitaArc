#include <stdlib.h>
#include <stdio.h>
#include <psp2/io/fcntl.h>

#include <common/types.h>
#include <common/define.h>
#include "logcat.h"

static SceUID logFile;
static bool logStarted = false;
static const char logLvString[] = {'V', 'I', 'W', 'E', 'F'};

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

void logBase(LOGLEVEL level, const char *tag, const char *format, va_list args)
{
  char logBuffer[1024] = {0x00};
  char *logPosition = logBuffer;

  logPosition += snprintf(logPosition, sizeof(logBuffer), "[%c] [%s] \t", logLvString[level], tag);
  logPosition += vsnprintf(logPosition, sizeof(logBuffer), format, args);
  logPosition += snprintf(logPosition, sizeof(logBuffer), "\n");

  sceIoWrite(logFile, logBuffer, logPosition - logBuffer);
}

void inline logV(const char *tag, const char *format, ...)
{
  VARG_WRAP(logBase(VERBOSE, tag, format, opt));
}

void inline logI(const char *tag, const char *format, ...)
{
  VARG_WRAP(logBase(INFORMATION, tag, format, opt));
}

void inline logW(const char *tag, const char *format, ...)
{
  VARG_WRAP(logBase(WARNING, tag, format, opt));
}

void inline logE(const char *tag, const char *format, ...)
{
  VARG_WRAP(logBase(ERROR, tag, format, opt));
}

void inline logF(const char *tag, const char *format, ...)
{
  VARG_WRAP(logBase(FATAL, tag, format, opt));
}
