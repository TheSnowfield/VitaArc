#ifndef __BRIDGES_JNI_THREAD_H
#define __BRIDGES_JNI_THREAD_H

#include <stdbool.h>
#include <pthread.h>
#include "aosp/jni.h"
#include "descriptor.h"

typedef struct jni_native_roots {
    struct jni_obj_t* objects[JNI_MAX_CALL_ARGS + 1];
    size_t count;
    struct jni_native_roots* parent;
} jni_native_roots_t;

typedef struct jni_thread {
    JNIEnv env;
    jthrowable exception;
    bool daemon;
    unsigned calls;
    unsigned lock_depth;
    uint64_t call_serial;
    struct jni_frame* frame;
    struct jni_pin* pins;
    jni_native_roots_t* native_roots;
    struct jni_thread* next;
} jni_thread_t;

void jni_thread_lock(void);
void jni_thread_unlock(void);
/* Suspend/resume an active JNI call with the runtime lock held. Object roots
 * and local-frame boundaries must be installed before releasing the lock. */
unsigned jni_thread_suspend(void);
void jni_thread_resume(unsigned depth);
void jni_thread_wait(void);
void jni_thread_notify(void);
bool jni_thread_enter(JNIEnv* env);
void jni_thread_leave(void);
jni_thread_t* jni_thread_current(void);
jni_thread_t* jni_thread_list(void);
bool jni_thread_version_supported(jint version);
jint jni_thread_attach(JNIEnv interface, bool daemon, JNIEnv** env);
jint jni_thread_detach(void);
jint jni_thread_destroy(void);
bool jni_thread_vm_alive(void);
const struct JNINativeInterface* jni_runtime_interface(void);

/* Called with the runtime lock held, including on native thread exit. */
void jni_runtime_thread_cleanup(jni_thread_t* thread);
void jni_runtime_collect(void);
void jni_runtime_vm_cleanup(void);

#endif /* __BRIDGES_JNI_THREAD_H */
