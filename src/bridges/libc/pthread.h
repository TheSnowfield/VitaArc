#ifndef _BRIDGE_LIBC_PTHREAD_H_
#define _BRIDGE_LIBC_PTHREAD_H_

#include <pthread.h>

int _pthread_create(pthread_t *tid,
                    const pthread_attr_t *attr,
                    void *(*start)(void *),
                    void *arg);

int _pthread_detach(pthread_t tid);

int _pthread_equal(pthread_t t1, pthread_t t2);

int _pthread_mutex_init(pthread_mutex_t *mutex,
                        const pthread_mutexattr_t *attr);

int _pthread_mutex_destroy(pthread_mutex_t *mutex);

int _pthread_mutex_lock(pthread_mutex_t *mutex);

int _pthread_mutex_timedlock(pthread_mutex_t *mutex,
                             const struct timespec *abstime);

int _pthread_mutex_trylock(pthread_mutex_t *mutex);

int _pthread_mutex_unlock(pthread_mutex_t *mutex);

int _pthread_mutexattr_init(pthread_mutexattr_t *attr);

int _pthread_mutexattr_destroy(pthread_mutexattr_t *attr);

int _pthread_key_create(pthread_key_t *key,
                        void (*destructor)(void *));

int _pthread_key_delete(pthread_key_t key);

int _pthread_setspecific(pthread_key_t key,
                         const void *value);

void *_pthread_getspecific(pthread_key_t key);

int _pthread_join(pthread_t thread,
                  void **value_ptr);

pthread_t _pthread_self(void);

int _pthread_once(pthread_once_t *once_control,
                  void (*init_routine)(void));

int _pthread_cond_destroy(pthread_cond_t *cond);

int _pthread_cond_wait(pthread_cond_t *cond,
                       pthread_mutex_t *mutex);

int _pthread_cond_timedwait(pthread_cond_t *cond,
                            pthread_mutex_t *mutex,
                            const struct timespec *abstime);

int _pthread_cond_signal(pthread_cond_t *cond);

int _pthread_cond_broadcast(pthread_cond_t *cond);

int _pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind);

#endif /* _BRIDGE_LIBC_PTHREAD_H_ */
