#include <stdlib.h>
#include <stdio.h>
#include <psp2/io/fcntl.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>

#include <common/types.h>
#include <common/define.h>
#include "logcat.h"

#define LOG_OVERUDP
#define UDP_PORT 23333
#define UDP_HOST "192.168.1.2"

static SceUID logStream;
static bool logStarted = false;
static const char logLvString[] = {'V', 'I', 'W', 'E', 'F'};
static void *logUdpBuffer = NULL;

void logBegin(const char *logFilePath)
{
  if (logStarted)
  {
    logW(TAG, "Calling logBegin() while logging.");
    return;
  }

#ifdef LOG_OVERUDP

  if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) != SCE_SYSMODULE_LOADED)
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);

  if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) == SCE_SYSMODULE_LOADED)
  {
    SceNetInAddr sNetInAddr = {0};
    SceNetCtlInfo sNetCtlInfo = {0};
    SceNetSockaddrIn sNetSockAddr = {0};
    SceNetInitParam sNetInitParam = {0};

    sNetInitParam.memory = malloc(65535);
    sNetInitParam.size = 65535;
    sNetInitParam.flags = 0;
    sNetSockAddr.sin_family = SCE_NET_AF_INET;
    sNetSockAddr.sin_port = sceNetHtons(UDP_PORT);

    sceNetInit(&sNetInitParam);
    sceNetCtlInit();
    sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &sNetCtlInfo);
    sceNetInetPton(SCE_NET_AF_INET, sNetCtlInfo.ip_address, &sNetInAddr);
    sceNetInetPton(SCE_NET_AF_INET, UDP_HOST, &sNetSockAddr.sin_addr);

    logStream = sceNetSocket("logcat", SCE_NET_AF_INET,
                             SCE_NET_SOCK_DGRAM, SCE_NET_IPPROTO_UDP);

    sceNetConnect(logStream, &sNetSockAddr, sizeof(sNetSockAddr));
  }

#else

  // Open log file
  logStream = sceIoOpen(logFilePath, SCE_O_CREAT | SCE_O_WRONLY, 0777);

#endif

  if (logStream <= 0)
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

  // Close log
#ifdef LOG_OVERUDP
  sceNetSocketClose(logStream);
#else
  sceIoClose(logStream);
#endif
}

void logBase(LOGLEVEL level, const char *tag, const char *format, va_list args)
{
  if (!logStarted)
    return;

  char logBuffer[1024] = {0x00};
  char *logPosition = logBuffer;

  logPosition += snprintf(logPosition, sizeof(logBuffer), "[%c] [%s] \t", logLvString[level], tag);
  logPosition += vsnprintf(logPosition, sizeof(logBuffer), format, args);
  logPosition += snprintf(logPosition, sizeof(logBuffer), "\n");

#ifdef LOG_OVERUDP
  sceNetSend(logStream, logBuffer, logPosition - logBuffer, 0);
#else
  sceIoWrite(logStream, logBuffer, logPosition - logBuffer);
#endif
}

void inline logPrintf(const char *format, ...)
{
  if (!logStarted)
    return;

  char logBuffer[1024] = {0x00};
  char *logPosition = logBuffer;

  va_list opt;
  va_start(opt, format);
  {
    logPosition += vsnprintf(logPosition, sizeof(logBuffer), format, opt);
#ifdef LOG_OVERUDP
    sceNetSend(logStream, logBuffer, logPosition - logBuffer, 0);
#else
    sceIoWrite(logStream, logBuffer, logPosition - logBuffer);
#endif
  }
  va_end(opt);
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
