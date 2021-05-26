#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include "../../common/define.h"
#include "../../logcat/logcat.h"
#include "pthread.h"

typedef struct
{
  pthread_mutex_t value;
} android_pthread_mutex_t;

#define _MUTEX(x) (((android_pthread_mutex_t *)x)->value)

int _pthread_create(pthread_t *tid,
                    const pthread_attr_t *attr,
                    void *(*start)(void *),
                    void *arg)
{
  logV(TAG, "_pthread_create(%d, %d, %d, %d)", tid, attr, start, arg);
  return pthread_create(tid, attr, start, arg);
}

int _pthread_detach(pthread_t tid)
{
  logV(TAG, "_pthread_detach(%d)", tid);
  return pthread_detach(tid);
}

int _pthread_equal(pthread_t t1,
                   pthread_t t2)
{
  logV(TAG, "_pthread_detach(%d, %d)", t1, t2);
  return pthread_equal(t1, t2);
}

int _pthread_mutex_init(pthread_mutex_t *mutex,
                        const pthread_mutexattr_t *attr)
{
  // validate mutex
  if (_MUTEX(mutex))
    return 0;

  logV(TAG, "_pthread_mutex_init(%d, %d)", _MUTEX(mutex), attr);

  // initialize the mutex
  return pthread_mutex_init(&_MUTEX(mutex), attr);
}

int _pthread_mutex_destroy(pthread_mutex_t *mutex)
{
  int nResult = pthread_mutex_destroy(&_MUTEX(mutex));
  {
    // cleanup
    _MUTEX(mutex) = NULL;
    logV(TAG, "_pthread_mutex_destroy(%d)", _MUTEX(mutex));
  }
  return nResult;
}

int _pthread_mutex_lock(pthread_mutex_t *mutex)
{
  logV(TAG, "_pthread_mutex_lock(%d)", _MUTEX(mutex));

  // try reinitialize the mutex
  _pthread_mutex_init(mutex, NULL);

  return pthread_mutex_lock(&_MUTEX(mutex));
}

int _pthread_mutex_timedlock(pthread_mutex_t *mutex,
                             const struct timespec *abstime)
{
  logV(TAG, "_pthread_mutex_timedlock(%d, %d)", _MUTEX(mutex), abstime);
  return pthread_mutex_timedlock(&_MUTEX(mutex), abstime);
}

int _pthread_mutex_trylock(pthread_mutex_t *mutex)
{
  logV(TAG, "_pthread_mutex_trylock(%d)", _MUTEX(mutex));
  return pthread_mutex_trylock(&_MUTEX(mutex));
}

int _pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  logV(TAG, "_pthread_mutex_unlock(%d)", _MUTEX(mutex));
  return pthread_mutex_unlock(&_MUTEX(mutex));
}

int _pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
  logV(TAG, "_pthread_mutexattr_init(%d)", attr);
  return pthread_mutexattr_init(attr);
}

int _pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
  logV(TAG, "_pthread_mutexattr_destroy(%d)", attr);
  return pthread_mutexattr_destroy(attr);
}

int _pthread_key_create(pthread_key_t *key,
                        void (*destructor)(void *))
{
  logV(TAG, "_pthread_key_create(%d, %d)", key, destructor);
  return pthread_key_create(key, destructor);
}

int _pthread_key_delete(pthread_key_t key)
{
  logV(TAG, "_pthread_key_delete(%d)", key);
  return pthread_key_delete(key);
}

int _pthread_setspecific(pthread_key_t key,
                         const void *value)
{
  logV(TAG, "_pthread_setspecific(%d, %d)", key, value);
  return pthread_setspecific(key, value);
}

void *_pthread_getspecific(pthread_key_t key)
{
  logV(TAG, "_pthread_getspecific(%d)", key);
  return pthread_getspecific(key);
}

int _pthread_join(pthread_t thread,
                  void **value_ptr)
{
  logV(TAG, "_pthread_join(%d, %d)", thread, value_ptr);
  return pthread_join(thread, value_ptr);
}

pthread_t _pthread_self(void)
{
  logV(TAG, "_pthread_self()");
  return pthread_self();
}

int _pthread_once(pthread_once_t *once_control,
                  void (*init_routine)(void))
{
  logV(TAG, "_pthread_once(%d, %d)", once_control, init_routine);
  return pthread_once(once_control, init_routine);
}

int _pthread_cond_destroy(pthread_cond_t *cond)
{
  logV(TAG, "_pthread_cond_destroy(%d)", cond);
  return pthread_cond_destroy(cond);
}

int _pthread_cond_wait(pthread_cond_t *cond,
                       pthread_mutex_t *mutex)
{
  logV(TAG, "_pthread_cond_wait(%d, %d)", cond, mutex);
  return pthread_cond_wait(cond, mutex);
}

int _pthread_cond_timedwait(pthread_cond_t *cond,
                            pthread_mutex_t *mutex,
                            const struct timespec *abstime)
{
  logV(TAG, "_pthread_cond_timedwait(%d, %d, %d)", cond, mutex, abstime);
  return pthread_cond_timedwait(cond, mutex, abstime);
}

int _pthread_cond_signal(pthread_cond_t *cond)
{
  logV(TAG, "_pthread_cond_signal(%d)", cond);
  return pthread_cond_signal(cond);
}

int _pthread_cond_broadcast(pthread_cond_t *cond)
{
  logV(TAG, "_pthread_cond_broadcast(%d)", cond);
  return pthread_cond_broadcast(cond);
}

int _pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind)
{
  logV(TAG, "_pthread_mutexattr_settype(%d, %d)", attr, kind);
  return pthread_mutexattr_settype(attr, kind);
}
