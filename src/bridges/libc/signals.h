#ifndef VITAARC_LIBC_SIGNALS_H
#define VITAARC_LIBC_SIGNALS_H

#include <stdint.h>

typedef void (*bridge_signal_t)(int);

int bridge_sigfillset(uint32_t* set);
int bridge_sigdelset(uint32_t* set, int signal);
int bridge_sigaction(int signal, const void* action, void* previous);
int bridge_sigprocmask(int how, const uint32_t* set, uint32_t* previous);
int bridge_register_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void), void* handle);

int bridge_sigsetjmp(void* state, int mask) __attribute__((returns_twice));
void bridge_siglongjmp(void* state, int value) __attribute__((noreturn));

#endif
