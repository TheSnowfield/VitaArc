#ifndef VITAARC_LIBC_SYNC_H
#define VITAARC_LIBC_SYNC_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef uint32_t bridge_mutex_t;
typedef uint32_t bridge_mutexattr_t;
typedef uint32_t bridge_cond_t;
typedef uint32_t bridge_condattr_t;

int _pthread_mutex_init(bridge_mutex_t* mutex, const bridge_mutexattr_t* attributes);
int _pthread_mutex_destroy(bridge_mutex_t* mutex);
int _pthread_mutex_lock(bridge_mutex_t* mutex);
int _pthread_mutex_trylock(bridge_mutex_t* mutex);
int _pthread_mutex_timedlock(bridge_mutex_t* mutex, const struct timespec* deadline);
int _pthread_mutex_unlock(bridge_mutex_t* mutex);
int _pthread_mutexattr_init(bridge_mutexattr_t* attributes);
int _pthread_mutexattr_destroy(bridge_mutexattr_t* attributes);
int _pthread_mutexattr_settype(bridge_mutexattr_t* attributes, int type);
int _pthread_mutexattr_gettype(const bridge_mutexattr_t* attributes, int* type);
int _pthread_mutexattr_setpshared(bridge_mutexattr_t* attributes, int shared);

int _pthread_cond_init(bridge_cond_t* condition, const bridge_condattr_t* attributes);
int _pthread_cond_destroy(bridge_cond_t* condition);
int _pthread_cond_signal(bridge_cond_t* condition);
int _pthread_cond_broadcast(bridge_cond_t* condition);
int _pthread_cond_wait(bridge_cond_t* condition, bridge_mutex_t* mutex);
int _pthread_cond_timedwait(bridge_cond_t* condition, bridge_mutex_t* mutex, const struct timespec* deadline);
int _pthread_condattr_init(bridge_condattr_t* attributes);
int _pthread_condattr_destroy(bridge_condattr_t* attributes);
int _pthread_condattr_setclock(bridge_condattr_t* attributes, int clock);

int bridge_sync_range_busy(uintptr_t base, size_t size);
int bridge_sync_destroy_range(uintptr_t base, size_t size);

#endif
