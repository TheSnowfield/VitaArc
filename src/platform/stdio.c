#include <errno.h>
#include <pthread.h>
#include <reent.h>
#include <stdio.h>
#include <stdlib.h>

/* This newlib build shares its FILE pool but omits locking in __sfp.
 * Keep slot allocation, release and lazy initialization under one lock.
 * File reads, writes and native opens remain outside this critical section. */
static pthread_mutex_t __pool = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;

FILE* __real___sfp(struct _reent* state);
void __real___sinit(struct _reent* state);

static void __lock(void)
{
    int error = errno;
    if(pthread_mutex_lock(&__pool) != 0) {
        /* Logging here could recursively allocate another FILE. */
        abort();
    }

    errno = error;
}

static void __unlock(void)
{
    int error = errno;
    if(pthread_mutex_unlock(&__pool) != 0) {
        abort();
    }

    errno = error;
}

FILE* __wrap___sfp(struct _reent* state)
{
    __lock();
    FILE* stream = __real___sfp(state);
    __unlock();
    return stream;
}

void __wrap___sinit(struct _reent* state)
{
    __lock();
    __real___sinit(state);
    __unlock();
}

void __wrap___sfp_lock_acquire(void)
{
    __lock();
}

void __wrap___sfp_lock_release(void)
{
    __unlock();
}
