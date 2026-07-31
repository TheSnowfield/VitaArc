#ifndef __BRIDGES_LIBC_IMPL_PTHREAD_H
#define __BRIDGES_LIBC_IMPL_PTHREAD_H

#include <pthread.h>
#include "once.h"
#include "sync.h"
#include "thread_attributes.h"

int _pthread_create(pthread_t* tid, const bridge_thread_attr_t* attr, void* (*start)(void*), void* arg);

int _pthread_detach(pthread_t tid);

int _pthread_equal(pthread_t t1, pthread_t t2);

int _pthread_key_create(pthread_key_t* key, void (*destructor)(void*));

int _pthread_key_delete(pthread_key_t key);

int _pthread_setspecific(pthread_key_t key, const void* value);

void* _pthread_getspecific(pthread_key_t key);

int _pthread_join(pthread_t thread, void** value_ptr);

pthread_t _pthread_self(void);

#endif /* __BRIDGES_LIBC_IMPL_PTHREAD_H */
