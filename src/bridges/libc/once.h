#ifndef VITAARC_LIBC_ONCE_H
#define VITAARC_LIBC_ONCE_H

#include <stdint.h>

typedef uint32_t bridge_once_t;

int _pthread_once(bridge_once_t* control, void (*initialize)(void));

#endif
