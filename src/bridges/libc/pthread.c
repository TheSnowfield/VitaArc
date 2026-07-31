#include <errno.h>

#include <logcat/logcat.h>
#include "pthread.h"

int _pthread_create(pthread_t* tid, const bridge_thread_attr_t* attr, void* (*start)(void*), void* arg)
{
    log_v(TAG, "_pthread_create(%d, %d, %d, %d)", tid, attr, start, arg);
    if(!tid || !start) {
        return EINVAL;
    }

#ifdef __vita__
    /* Android workers expect a 1 MiB default, while this SDK defaults to 32 KiB.
     * Guard pages are unavailable here; explicit nonzero requests are rejected. */
    const bridge_thread_attr_t defaults = {.stack_size = 1024 * 1024};

    if(!attr) {
        attr = &defaults;
    }

#endif
    pthread_attr_t native;
    int result = attr ? bridge_thread_native_attributes(&native, attr) : 0;
    if(result) {
        return result;
    }

    pthread_t created;
    result = pthread_create(&created, attr ? &native : NULL, start, arg);
    if(attr) {
        pthread_attr_destroy(&native);
    }

    if(!result) {
        *tid = created;
    }

    return result;
}

int _pthread_detach(pthread_t tid)
{
    log_v(TAG, "_pthread_detach(%d)", tid);
    return pthread_detach(tid);
}

int _pthread_equal(pthread_t t1, pthread_t t2)
{
    log_v(TAG, "_pthread_equal(%d, %d)", t1, t2);
    /* Bionic compares opaque ID values, including zero-initialized C++ IDs.
     * This SDK dereferences equal IDs and faults when both are zero. */
    return t1 == t2;
}

int _pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
    log_v(TAG, "_pthread_key_create(%d, %d)", key, destructor);
    return pthread_key_create(key, destructor);
}

int _pthread_key_delete(pthread_key_t key)
{
    log_v(TAG, "_pthread_key_delete(%d)", key);
    return pthread_key_delete(key);
}

int _pthread_setspecific(pthread_key_t key, const void* value)
{
    log_v(TAG, "_pthread_setspecific(%d, %d)", key, value);
    return pthread_setspecific(key, value);
}

void* _pthread_getspecific(pthread_key_t key)
{
    log_v(TAG, "_pthread_getspecific(%d)", key);
    return pthread_getspecific(key);
}

int _pthread_join(pthread_t thread, void** value_ptr)
{
    log_v(TAG, "_pthread_join(%d, %d)", thread, value_ptr);
    return pthread_join(thread, value_ptr);
}

pthread_t _pthread_self(void)
{
    log_v(TAG, "_pthread_self()");
    return pthread_self();
}
