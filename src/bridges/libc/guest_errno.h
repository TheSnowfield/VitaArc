#ifndef VITAARC_GUEST_ERRNO_H
#define VITAARC_GUEST_ERRNO_H

#include <stddef.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <locale.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <wchar.h>
#include <abi/calling.h>
#include <compat/compat.h>
#include "directory.h"
#include "file.h"
#include "memory.h"
#include "posix.h"
#include "pthread.h"
#include "locale.h"
#include "fortify.h"
#include "auxv.h"
#include "walk.h"
#include "signals.h"

int bridge_errno_from_native(int error);
int bridge_errno_to_native(int error);

#define GUEST_ERRNO(result, name, target, parameters, arguments) result VITAARC_SOFTFP bridge_guest_##name parameters;
#define GUEST_CODE(result, name, target, parameters, arguments) GUEST_ERRNO(result, name, target, parameters, arguments)
#define GUEST_VOID(name, target, parameters, arguments) void VITAARC_SOFTFP bridge_guest_##name parameters;
#include "guest_errno.def"
#undef GUEST_ERRNO
#undef GUEST_CODE
#undef GUEST_VOID

int bridge_guest_open(const char* path, int flags, ...);
int bridge_guest_fcntl(int fd, int command, ...);
int bridge_guest_ioctl(int fd, int request, ...);
long bridge_guest_syscall(long number, ...);
void* bridge_guest_mremap(void* address, size_t old_size, size_t new_size, int flags, ...);
char* bridge_guest_strerror(int error);
int bridge_guest_strerror_r(int error, char* buffer, size_t length);
void bridge_guest_perror(const char* prefix);
int bridge_guest_fprintf(FILE* file, const char* format, ...);
int bridge_guest_sprintf(char* buffer, const char* format, ...);
int bridge_guest_snprintf(char* buffer, size_t size, const char* format, ...);
int bridge_guest_sscanf(const char* text, const char* format, ...);
int bridge_guest_pthread_once(bridge_once_t* control, void (*initialize)(void));

#endif
