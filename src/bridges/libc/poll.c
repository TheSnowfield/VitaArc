#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "poll.h"

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/net/net.h>
#include <platform/newlib.h>
#include "posix.h"
#else
#include <poll.h>
#endif

_Static_assert(sizeof(bridge_pollfd_t) == 8, "Android pollfd size");
_Static_assert(offsetof(bridge_pollfd_t, returned) == 6, "Android pollfd revents offset");

#ifdef __vita__
#define READ_MASK (BIONIC_POLLIN | BIONIC_POLLRDNORM)
#define WRITE_MASK (BIONIC_POLLOUT | BIONIC_POLLWRNORM)

typedef struct socket_poll {
    int uid;
    unsigned interest;
    unsigned returned;
} socket_poll_t;

typedef struct poll_state {
    vita_newlib_descriptor_t* references[BRIDGE_POLL_MAX];
    unsigned short socket_index[BRIDGE_POLL_MAX];
    socket_poll_t sockets[BRIDGE_POLL_MAX];
    SceNetEpollEvent events[BRIDGE_POLL_MAX];
} poll_state_t;

static int __empty_wait(int timeout_ms)
{
    if(timeout_ms >= 0) {
        struct timespec duration = {.tv_sec = timeout_ms / 1000, .tv_nsec = (timeout_ms % 1000) * 1000000L};

        return bridge_nanosleep(&duration, NULL);
    }

    for(;;) {
        const struct timespec duration = {.tv_sec = 3600};

        if(bridge_nanosleep(&duration, NULL) < 0) {
            return -1;
        }
    }
}

static unsigned __interest(short events)
{
    unsigned result = SCE_NET_EPOLLERR | SCE_NET_EPOLLHUP;
    if(events & READ_MASK) {
        result |= SCE_NET_EPOLLIN;
    }

    if(events & WRITE_MASK) {
        result |= SCE_NET_EPOLLOUT;
    }

    return result;
}

static int __native_poll(bridge_pollfd_t* descriptors, unsigned count, int timeout_ms)
{
    int previous_error = errno;
    if(!count) {
        int result = __empty_wait(timeout_ms);
        if(!result) {
            errno = previous_error;
        }

        return result;
    }

    poll_state_t* state = calloc(1, sizeof(*state));
    if(!state) {
        return -1;
    }

    unsigned socket_count = 0;
    int result = -1, error = 0, epoll = -1, immediate = 0;
    uint64_t started = sceKernelGetProcessTimeWide();
    uint64_t duration = timeout_ms >= 0 ? (uint64_t)timeout_ms * 1000 : 0;
    for(unsigned index = 0; index < count; ++index) {
        bridge_pollfd_t* entry = &descriptors[index];
        if(entry->fd < 0) {
            continue;
        }

        vita_newlib_descriptor_t* reference = __vita_fd_grab(entry->fd);
        state->references[index] = reference;
        if(!reference) {
            entry->returned = BIONIC_POLLNVAL;
            ++immediate;
            continue;
        }

        switch(reference->type) {
            case 0:
            case 1:
                /* Regular files/directories do not wait for socket readiness. */
                entry->returned = entry->events & (READ_MASK | WRITE_MASK);
                break;

            case VITAARC_DESCRIPTOR_RANDOM:
                entry->returned = entry->events & READ_MASK;
                break;

            case 2: {
                if(entry->events & (BIONIC_POLLPRI | BIONIC_POLLRDBAND | BIONIC_POLLWRBAND | BIONIC_POLLRDHUP)) {
                    error = ENOTSUP;
                    goto cleanup;
                }

                unsigned slot = 0;
                while(slot < socket_count && state->sockets[slot].uid != reference->uid) {
                    ++slot;
                }

                if(slot == socket_count) {
                    state->sockets[slot].uid = reference->uid;
                    ++socket_count;
                }

                state->sockets[slot].interest |= __interest(entry->events);
                state->socket_index[index] = slot + 1;
                break;
            }

            default:
                /* Message pipes/TTYs require their own native waiting backend. */
                error = ENOTSUP;
                goto cleanup;
        }

        if(entry->returned) {
            ++immediate;
        }
    }

    if(!socket_count) {
        result = immediate ? immediate : __empty_wait(timeout_ms);
        if(result < 0) {
            error = errno;
        }

        goto cleanup;
    }

    epoll = sceNetEpollCreate("guest_poll", 0);
    if(epoll < 0) {
        error = __vita_sce_errno_to_errno(epoll, 1);
        goto cleanup;
    }

    for(unsigned slot = 0; slot < socket_count; ++slot) {
        SceNetEpollEvent event = {0};

        event.events = state->sockets[slot].interest;
        event.data.fd = slot;
        int status = sceNetEpollControl(epoll, SCE_NET_EPOLL_CTL_ADD, state->sockets[slot].uid, &event);
        if(status < 0) {
            error = __vita_sce_errno_to_errno(status, 1);
            goto cleanup;
        }
    }

    for(;;) {
        int wait_us;
        if(immediate || timeout_ms == 0) {
            wait_us = 0;
        } else if(timeout_ms < 0) {
            wait_us = -1;
        } else {
            uint64_t elapsed = sceKernelGetProcessTimeWide() - started;
            uint64_t remaining = elapsed < duration ? duration - elapsed : 0;
            wait_us = remaining > INT_MAX ? INT_MAX : (int)remaining;
        }

        int events = sceNetEpollWait(epoll, state->events, socket_count, wait_us);
        if(events < 0) {
            error = __vita_sce_errno_to_errno(events, 1);
            goto cleanup;
        }

        if((unsigned)events > socket_count) {
            error = EIO;
            goto cleanup;
        }

        for(int index = 0; index < events; ++index) {
            SceNetEpollEvent* event = &state->events[index];
            if(event->data.fd < 0 || (unsigned)event->data.fd >= socket_count) {
                error = EIO;
                goto cleanup;
            }

            state->sockets[event->data.fd].returned |= event->events;
        }

        result = 0;
        for(unsigned index = 0; index < count; ++index) {
            unsigned slot = state->socket_index[index];
            bridge_pollfd_t* entry = &descriptors[index];
            if(slot) {
                unsigned flags = state->sockets[slot - 1].returned;
                if(flags & SCE_NET_EPOLLIN) {
                    entry->returned |= entry->events & READ_MASK;
                }

                if(flags & SCE_NET_EPOLLOUT) {
                    entry->returned |= entry->events & WRITE_MASK;
                }

                if(flags & SCE_NET_EPOLLERR) {
                    entry->returned |= BIONIC_POLLERR;
                }

                if(flags & SCE_NET_EPOLLHUP) {
                    entry->returned |= BIONIC_POLLHUP;
                }
            }

            if(entry->returned) {
                ++result;
            }
        }

        if(result || timeout_ms == 0 || (timeout_ms > 0 && sceKernelGetProcessTimeWide() - started >= duration)) {
            break;
        }
    }

cleanup:
    if(epoll >= 0) {
        int status = sceNetEpollDestroy(epoll);
        if(status < 0 && !error) {
            error = __vita_sce_errno_to_errno(status, 1);
        }
    }

    for(unsigned index = 0; index < count; ++index) {
        if(state->references[index]) {
            __vita_fd_drop(state->references[index]);
        }

        if(error) {
            descriptors[index].returned = 0;
        }
    }

    free(state);
    errno = error ? error : previous_error;
    return error ? -1 : result;
}

#else
typedef struct {
    short android;
    short native;
} poll_pair_t;

static const poll_pair_t __events[] = {
    {BIONIC_POLLIN, POLLIN},
    {BIONIC_POLLPRI, POLLPRI},
    {BIONIC_POLLOUT, POLLOUT},
    {BIONIC_POLLERR, POLLERR},
    {BIONIC_POLLHUP, POLLHUP},
    {BIONIC_POLLNVAL, POLLNVAL},
    {BIONIC_POLLRDNORM, POLLRDNORM},
    {BIONIC_POLLRDBAND, POLLRDBAND},
    {BIONIC_POLLWRNORM, POLLWRNORM},
    {BIONIC_POLLWRBAND, POLLWRBAND},
#ifdef POLLRDHUP
    {BIONIC_POLLRDHUP, POLLRDHUP},
#endif
};

static int __native_poll(bridge_pollfd_t* descriptors, unsigned count, int timeout_ms)
{
    struct pollfd native[BRIDGE_POLL_MAX];
    for(unsigned index = 0; index < count; ++index) {
        native[index] = (struct pollfd){.fd = descriptors[index].fd};

        for(unsigned flag = 0; flag < sizeof(__events) / sizeof(__events[0]); ++flag) {
            if(descriptors[index].events & __events[flag].android) {
                native[index].events |= __events[flag].native;
            }
        }
    }

    int result = poll(native, count, timeout_ms);
    if(result >= 0) {
        for(unsigned index = 0; index < count; ++index) {
            for(unsigned flag = 0; flag < sizeof(__events) / sizeof(__events[0]); ++flag) {
                if(native[index].revents & __events[flag].native) {
                    descriptors[index].returned |= __events[flag].android;
                }
            }
        }
    }

    return result;
}

#endif

int bridge_poll(void* descriptors, unsigned count, int timeout_ms)
{
    if(count > BRIDGE_POLL_MAX) {
        errno = EINVAL;
        return -1;
    }

    if(!descriptors && count) {
        errno = EFAULT;
        return -1;
    }

    bridge_pollfd_t* entries = descriptors;
    for(unsigned index = 0; index < count; ++index) {
        entries[index].returned = 0;
    }

    return __native_poll(entries, count, timeout_ms);
}
