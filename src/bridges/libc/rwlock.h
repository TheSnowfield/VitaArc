#ifndef VITAARC_LIBC_RWLOCK_H
#define VITAARC_LIBC_RWLOCK_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t bridge_rwlock_t;

int bridge_rwlock_init(bridge_rwlock_t* lock, const uint32_t* attributes);
int bridge_rwlock_destroy(bridge_rwlock_t* lock);
int bridge_rwlock_read(bridge_rwlock_t* lock);
int bridge_rwlock_write(bridge_rwlock_t* lock);
int bridge_rwlock_unlock(bridge_rwlock_t* lock);

int bridge_rwlock_range(uintptr_t base, size_t size, int destroy);

#endif
