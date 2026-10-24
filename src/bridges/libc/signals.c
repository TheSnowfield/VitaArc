#include <errno.h>
#include "signals.h"

int bridge_sigfillset(uint32_t* set)
{
    if(!set) {
        errno = EINVAL;
        return -1;
    }

    *set = UINT32_MAX;
    return 0;
}

int bridge_sigdelset(uint32_t* set, int signal)
{
    if(!set || signal < 1 || signal > 32) {
        errno = EINVAL;
        return -1;
    }

    *set &= ~(UINT32_C(1) << (signal - 1));
    return 0;
}

int bridge_sigaction(int signal, const void* action, void* previous)
{
    (void)signal;
    (void)action;
    (void)previous;
    errno = ENOSYS;
    return -1;
}

int bridge_sigprocmask(int how, const uint32_t* set, uint32_t* previous)
{
    (void)how;
    (void)set;
    (void)previous;
    errno = ENOSYS;
    return -1;
}

int bridge_register_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void), void* handle)
{
    (void)prepare;
    (void)parent;
    (void)child;
    (void)handle;
    return ENOSYS;
}

