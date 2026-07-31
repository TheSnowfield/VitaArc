#ifndef VITAARC_LIBC_POLL_H
#define VITAARC_LIBC_POLL_H

#include <stdint.h>

typedef struct bridge_pollfd {
    int32_t fd;
    int16_t events;
    int16_t returned;
} bridge_pollfd_t;

enum {
    BIONIC_POLLIN = 0x001,
    BIONIC_POLLPRI = 0x002,
    BIONIC_POLLOUT = 0x004,
    BIONIC_POLLERR = 0x008,
    BIONIC_POLLHUP = 0x010,
    BIONIC_POLLNVAL = 0x020,
    BIONIC_POLLRDNORM = 0x040,
    BIONIC_POLLRDBAND = 0x080,
    BIONIC_POLLWRNORM = 0x100,
    BIONIC_POLLWRBAND = 0x200,
    BIONIC_POLLRDHUP = 0x2000,
    BRIDGE_POLL_MAX = 256
};

int bridge_poll(void* descriptors, unsigned int count, int timeout_ms);

#endif
