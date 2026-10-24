#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "guest_errno.h"

/* GNU extension with the same va_list ABI on newlib and the host. Avoid
 * enabling glibc's incompatible GNU strerror_r prototype in this unit. */
extern int vasprintf(char** buffer, const char* format, va_list arguments);
extern double strtod_l(const char* text, char** end, locale_t locale);
extern long long strtoll_l(const char* text, char** end, int base, locale_t locale);
extern unsigned long long strtoull_l(const char* text, char** end, int base, locale_t locale);
extern int usleep(useconds_t delay);
extern int getentropy(void* buffer, size_t length);

typedef struct {
    int native;
    int android;
} errno_pair_t;

static const errno_pair_t __errors[] = {
    {0, 0},
#ifdef ENOTSUP
    {ENOTSUP, 95},
#endif
#ifdef EWOULDBLOCK
    {EWOULDBLOCK, 11},
#endif
#ifdef EDEADLOCK
    {EDEADLOCK, 35},
#endif
#include "errno_values.def"
};

int bridge_errno_from_native(int error)
{
    for(size_t index = 0; index < sizeof(__errors) / sizeof(__errors[0]); ++index) {
        if(error == __errors[index].native) {
            return __errors[index].android;
        }
    }

    return 5; /* An unrecognized native error is an I/O error, not a Linux code. */
}

int bridge_errno_to_native(int error)
{
    for(size_t index = 0; index < sizeof(__errors) / sizeof(__errors[0]); ++index) {
        if(error == __errors[index].android) {
            return __errors[index].native;
        }
    }

    return error;
}

typedef struct errno_scope {
    int android;
    int native;
} errno_scope_t;

static errno_scope_t __enter(void)
{
    errno_scope_t scope = {.android = errno};

    /* These APIs do not take errno as input. Zero distinguishes a fresh native
     * error from an unchanged caller value, including unknown guest numbers. */
    scope.native = 0;
    errno = scope.native;
    return scope;
}

static void __leave(errno_scope_t scope)
{
    errno = errno == scope.native ? scope.android : bridge_errno_from_native(errno);
}

#define GUEST_ERRNO(type, name, target, parameters, arguments)                                                         \
    type VITAARC_SOFTFP bridge_guest_##name parameters                                                                 \
    {                                                                                                                  \
        errno_scope_t scope = __enter();                                                                               \
        type result_value = target arguments;                                                                          \
        __leave(scope);                                                                                                \
        return result_value;                                                                                           \
    }
#define GUEST_CODE(type, name, target, parameters, arguments)                                                          \
    type VITAARC_SOFTFP bridge_guest_##name parameters                                                                 \
    {                                                                                                                  \
        int saved_error = errno;                                                                                       \
        errno = 0;                                                                                                     \
        type result_value = target arguments;                                                                          \
        errno = saved_error;                                                                                           \
        return bridge_errno_from_native(result_value);                                                                 \
    }
#define GUEST_VOID(name, target, parameters, arguments)                                                                \
    void VITAARC_SOFTFP bridge_guest_##name parameters                                                                 \
    {                                                                                                                  \
        errno_scope_t scope = __enter();                                                                               \
        target arguments;                                                                                              \
        __leave(scope);                                                                                                \
    }
#include "guest_errno.def"
#undef GUEST_ERRNO
#undef GUEST_CODE
#undef GUEST_VOID

int bridge_guest_open(const char* path, int flags, ...)
{
    unsigned int mode = 0;
    if(flags & BIONIC_O_CREAT) {
        va_list arguments;
        va_start(arguments, flags);
        mode = va_arg(arguments, unsigned int);
        va_end(arguments);
    }

    errno_scope_t scope = __enter();
    int result = bridge_open(path, flags, mode);
    __leave(scope);
    return result;
}

int bridge_guest_fcntl(int fd, int command, ...)
{
    int argument = 0;
    if(command == 0 || command == 2 || command == 4 || command == 1030) {
        va_list arguments;
        va_start(arguments, command);
        argument = va_arg(arguments, int);
        va_end(arguments);
    }

    errno_scope_t scope = __enter();
    int result = bridge_fcntl(fd, command, argument);
    __leave(scope);
    return result;
}

int bridge_guest_ioctl(int fd, int request, ...)
{
    errno_scope_t scope = __enter();
    int result = bridge_ioctl(fd, request);
    __leave(scope);
    return result;
}

long bridge_guest_syscall(long number, ...)
{
    errno_scope_t scope = __enter();
    long result = bridge_syscall(number);
    __leave(scope);
    return result;
}

void* bridge_guest_mremap(void* address, size_t old_size, size_t new_size, int flags, ...)
{
    /* The internal implementation rejects fixed remap before consuming an
     * optional address. It must be forwarded if fixed remap gains support. */
    errno_scope_t scope = __enter();
    void* result = bridge_mremap(address, old_size, new_size, flags);
    __leave(scope);
    return result;
}

char* bridge_guest_strerror(int error)
{
    errno_scope_t scope = __enter();
    char* result = strerror(bridge_errno_to_native(error));
    __leave(scope);
    return result;
}

int bridge_guest_strerror_r(int error, char* buffer, size_t length)
{
    int saved = errno;
    int result = strerror_r(bridge_errno_to_native(error), buffer, length);
    errno = saved;
    return bridge_errno_from_native(result);
}

void bridge_guest_perror(const char* prefix)
{
    errno_scope_t scope = __enter();
    scope.native = bridge_errno_to_native(scope.android);
    errno = scope.native;
    perror(prefix);
    __leave(scope);
}

int bridge_guest_fprintf(FILE* file, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = bridge_guest_vfprintf(file, format, arguments);
    va_end(arguments);
    return result;
}

int bridge_guest_sprintf(char* buffer, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = bridge_guest_vsprintf(buffer, format, arguments);
    va_end(arguments);
    return result;
}

int bridge_guest_snprintf(char* buffer, size_t size, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = bridge_guest_vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    return result;
}

int bridge_guest_sscanf(const char* text, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = bridge_guest_vsscanf(text, format, arguments);
    va_end(arguments);
    return result;
}

int bridge_guest_pthread_once(bridge_once_t* control, void (*initialize)(void))
{
    /* The helper preserves errno except for the user's initializer, whose errno
     * changes must remain visible to that initializing thread. */
    return bridge_errno_from_native(_pthread_once(control, initialize));
}
