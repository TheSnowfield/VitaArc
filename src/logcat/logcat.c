#include <stdlib.h>
#include <stdio.h>
#include <psp2/io/fcntl.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/sysmodule.h>

#include <types.h>
#include <config.h>
#include "logcat.h"

#define LOG_OVERUDP
#define UDP_PORT 23333
#define UDP_HOST "10.20.0.227"

static SceUID __log_stream;
static bool __log_started = false;
static const char __log_level_string[] = {'V', 'I', 'W', 'E', 'F'};

void log_begin(const char *log_file_path)
{
  if (__log_started)
  {
    log_w(TAG, "Calling log_begin() while logging.");
    return;
  }

#ifdef LOG_OVERUDP

  if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) != SCE_SYSMODULE_LOADED)
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);

  if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) == SCE_SYSMODULE_LOADED)
  {
    SceNetInAddr net_address = {0};
    SceNetCtlInfo net_control_info = {0};
    SceNetSockaddrIn socket_address = {0};
    SceNetInitParam init_parameters = {0};

    init_parameters.memory = malloc(65535);
    init_parameters.size = 65535;
    init_parameters.flags = 0;
    socket_address.sin_family = SCE_NET_AF_INET;
    socket_address.sin_port = sceNetHtons(UDP_PORT);

    sceNetInit(&init_parameters);
    sceNetCtlInit();
    sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &net_control_info);
    sceNetInetPton(SCE_NET_AF_INET, net_control_info.ip_address, &net_address);
    sceNetInetPton(SCE_NET_AF_INET, UDP_HOST, &socket_address.sin_addr);

    __log_stream = sceNetSocket("logcat", SCE_NET_AF_INET,
                                SCE_NET_SOCK_DGRAM, SCE_NET_IPPROTO_UDP);

    sceNetConnect(__log_stream, (const SceNetSockaddr *)&socket_address,
                  sizeof(socket_address));
  }

#else

  // Open log file
  __log_stream = sceIoOpen(log_file_path, SCE_O_CREAT | SCE_O_WRONLY, 0777);

#endif

  if (__log_stream <= 0)
    return;

  __log_started = true;
  log_i(TAG, "Log started.");
}

void log_end(void)
{
  if (!__log_started)
    return;

  // Stop log
  __log_started = false;
  log_i(TAG, "Log stop.");

  // Close log
#ifdef LOG_OVERUDP
  sceNetSocketClose(__log_stream);
#else
  sceIoClose(__log_stream);
#endif
}

void log_base(LOGLEVEL level, const char *tag, const char *format, va_list args)
{
  if (!__log_started)
    return;

  char log_buffer[1024] = {0x00};
  char *log_position = log_buffer;

  log_position += snprintf(log_position, sizeof(log_buffer), "[%c] [%s] \t",
                           __log_level_string[level], tag);
  log_position += vsnprintf(log_position, sizeof(log_buffer), format, args);
  log_position += snprintf(log_position, sizeof(log_buffer), "\n");

#ifdef LOG_OVERUDP
  sceNetSend(__log_stream, log_buffer, log_position - log_buffer, 0);
  sceKernelDelayThread(50);
#else
  sceIoWrite(__log_stream, log_buffer, log_position - log_buffer);
#endif
}

void inline log_printf(const char *format, ...)
{
  if (!__log_started)
    return;

  char log_buffer[1024] = {0x00};
  char *log_position = log_buffer;

  va_list opt;
  va_start(opt, format);
  {
    log_position += vsnprintf(log_position, sizeof(log_buffer), format, opt);
#ifdef LOG_OVERUDP
    sceNetSend(__log_stream, log_buffer, log_position - log_buffer, 0);
#else
    sceIoWrite(__log_stream, log_buffer, log_position - log_buffer);
#endif
  }
  va_end(opt);
}

void inline log_v(const char *tag, const char *format, ...)
{
  VARG_WRAP(log_base(VERBOSE, tag, format, opt));
}

void inline log_i(const char *tag, const char *format, ...)
{
  VARG_WRAP(log_base(INFORMATION, tag, format, opt));
}

void inline log_w(const char *tag, const char *format, ...)
{
  VARG_WRAP(log_base(WARNING, tag, format, opt));
}

void inline log_e(const char *tag, const char *format, ...)
{
  VARG_WRAP(log_base(ERROR, tag, format, opt));
}

void inline log_f(const char *tag, const char *format, ...)
{
  VARG_WRAP(log_base(FATAL, tag, format, opt));
}
