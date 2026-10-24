#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <psp2/io/fcntl.h>
#ifdef LOG_OVERUDP
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#endif
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>
#include <string.h>

#include <types.h>
#include <config.h>
#include "logcat.h"

/* File logging is the default. UDP diagnostics require an explicit destination
 * supplied by the developer build, rather than a machine-specific address. */
#if defined(LOG_OVERUDP) && !defined(UDP_HOST)
#error "LOG_OVERUDP requires an explicit UDP_HOST"
#endif
#define UDP_PORT 23333
#define LOG_BATCH (64 * 1024)

static SceUID __log_stream = -1;
static atomic_bool __log_started;
static const char __log_level_string[] = {'V', 'I', 'W', 'E', 'F'};
static pthread_mutex_t __mutex = PTHREAD_MUTEX_INITIALIZER;
static _Atomic(log_view_t) __view;

#ifndef LOG_OVERUDP
static char __batch[LOG_BATCH];
static size_t __fill;

/* The sink lock is held. A failed sink must not stall application startup. */
static void __flush(void)
{
    size_t sent = 0;
    while(sent < __fill) {
        int result = sceIoWrite(__log_stream, __batch + sent, __fill - sent);
        if(result <= 0) {
            break;
        }

        sent += (size_t)result;
    }

    __fill = 0;
}
#endif

void log_set_view(log_view_t view)
{
    atomic_store(&__view, view);
}

void log_flush(void)
{
    if(!__log_started) {
        return;
    }

    pthread_mutex_lock(&__mutex);
#ifndef LOG_OVERUDP
    if(__log_started) {
        __flush();
    }
#endif
    pthread_mutex_unlock(&__mutex);
}

static void __write(log_level_t level, const char* text, size_t size)
{
    log_view_t view = atomic_load(&__view);
    if(view) {
        view(level, text, size);
    }

    pthread_mutex_lock(&__mutex);
    if(!__log_started) {
        pthread_mutex_unlock(&__mutex);
        return;
    }

#ifdef LOG_OVERUDP
    sceNetSend(__log_stream, text, size, 0);
#else
    while(size) {
        size_t room = sizeof(__batch) - __fill;
        size_t count = size < room ? size : room;
        memcpy(__batch + __fill, text, count);
        __fill += count;
        text += count;
        size -= count;
        if(__fill == sizeof(__batch)) {
            __flush();
        }
    }

    if(level >= ERROR) {
        __flush();
    }
#endif
    pthread_mutex_unlock(&__mutex);
}

static void __intro(void)
{
    static const char text[] =
        "LICENSE / Credits\n"
        "vitaGL / vitaShaRK - Rinnegatamante / contributors - LGPL-3.0-or-later\n"
        "FreeType - The FreeType Project - FreeType License\n"
        "libogg / libvorbis - Xiph.Org Foundation - BSD-3-Clause\n"
        "libpng - PNG Reference Library License\n"
        "zlib - zlib License / bzip2 - bzip2 License\n";

    sceClibPrintf("%s", text);
    __write(INFORMATION, text, sizeof(text) - 1);
}

void log_begin(const char* log_file_path)
{
    pthread_mutex_lock(&__mutex);
    if(__log_started) {
        pthread_mutex_unlock(&__mutex);
        log_w(TAG, "Calling log_begin() while logging.");
        return;
    }

#ifdef LOG_OVERUDP

    if(sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) != SCE_SYSMODULE_LOADED) {
        sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    }

    if(sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) == SCE_SYSMODULE_LOADED) {
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

        __log_stream = sceNetSocket("logcat", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, SCE_NET_IPPROTO_UDP);

        sceNetConnect(__log_stream, (const SceNetSockaddr*)&socket_address, sizeof(socket_address));
    }

#else

    // Open log file
    if(log_file_path) {
        __log_stream = sceIoOpen(log_file_path, SCE_O_CREAT | SCE_O_WRONLY | SCE_O_TRUNC, 0666);
    }

#endif

    if(__log_stream < 0) {
        pthread_mutex_unlock(&__mutex);
        __intro();
        return;
    }

    __log_started = true;
    pthread_mutex_unlock(&__mutex);
    __intro();
    log_i(TAG, "Log started.");
}

void log_end(void)
{
    if(!__log_started) {
        return;
    }

    // Stop log
    log_i(TAG, "Log stop.");
    pthread_mutex_lock(&__mutex);
    if(!__log_started) {
        pthread_mutex_unlock(&__mutex);
        return;
    }

#ifndef LOG_OVERUDP
    __flush();
#endif
    __log_started = false;

    // Close log
#ifdef LOG_OVERUDP
    sceNetSocketClose(__log_stream);
#else
    sceIoClose(__log_stream);
#endif
    __log_stream = -1;
    pthread_mutex_unlock(&__mutex);
}

void log_base(log_level_t level, const char* tag, const char* format, va_list args)
{
#if !VITAARC_VERBOSE_LOGS
    if(level == VERBOSE) {
        return;
    }
#endif

    if(!__log_started && level == VERBOSE && !atomic_load(&__view)) {
        return;
    }

    if((unsigned)level > FATAL || !format) {
        return;
    }

    char log_buffer[1024];
    int prefix =
        snprintf(log_buffer, sizeof(log_buffer) - 1, "[%c] [%s] \t", __log_level_string[level], tag ? tag : "(null)");
    if(prefix < 0) {
        return;
    }

    size_t used = (size_t)prefix < sizeof(log_buffer) - 2 ? (size_t)prefix : sizeof(log_buffer) - 2;
    size_t available = sizeof(log_buffer) - 1 - used;
    int body = vsnprintf(log_buffer + used, available, format, args);
    if(body < 0) {
        return;
    }

    used += (size_t)body < available ? (size_t)body : available - 1;
    log_buffer[used++] = '\n';
    log_buffer[used] = 0;
    /* System logging remains available when the optional UDP/file sink fails. */
    if(level >= INFORMATION) {
        sceClibPrintf("%s", log_buffer);
    }

    __write(level, log_buffer, used);
}

inline void log_printf(const char* format, ...)
{
#if !VITAARC_VERBOSE_LOGS
    return;
#endif

    if(!__log_started && !atomic_load(&__view)) {
        return;
    }

    char log_buffer[1024];

    va_list opt;
    va_start(opt, format);
    {
        int result = vsnprintf(log_buffer, sizeof(log_buffer), format, opt);
        size_t length = result < 0 ? 0 : strlen(log_buffer);
        __write(VERBOSE, log_buffer, length);
    }

    va_end(opt);
}

inline void log_v(const char* tag, const char* format, ...)
{
    VARG_WRAP(log_base(VERBOSE, tag, format, opt));
}

inline void log_i(const char* tag, const char* format, ...)
{
    VARG_WRAP(log_base(INFORMATION, tag, format, opt));
}

inline void log_w(const char* tag, const char* format, ...)
{
    VARG_WRAP(log_base(WARNING, tag, format, opt));
}

inline void log_e(const char* tag, const char* format, ...)
{
    VARG_WRAP(log_base(ERROR, tag, format, opt));
}

inline void log_f(const char* tag, const char* format, ...)
{
    VARG_WRAP(log_base(FATAL, tag, format, opt));
}
