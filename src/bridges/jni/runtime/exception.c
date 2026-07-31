#include <logcat/logcat.h>

#include "object.h"
#include "entry.h"
#include "class.h"
#include "exception.h"

void jni_raise(JNIEnv* env, const char* class_name, const char* message)
{
    if(JNI_PENDING_EXCEPTION) {
        return;
    }

    jni_entry_throw_new(env, jni_entry_find_class(env, class_name), message);
}

jint jni_entry_throw(JNIEnv* env, jthrowable throwable)
{
    (void)env;
    JNI_PENDING_EXCEPTION = (jthrowable)jni_obj_get(throwable);
    return JNI_OK;
}

jint jni_entry_throw_new(JNIEnv* env, jclass clazz, const char* message)
{
    JNI_PENDING_EXCEPTION = (jthrowable)jni_obj_new(env, clazz);
    jni_obj_t* exception = jni_obj_get(JNI_PENDING_EXCEPTION);
    if(exception) {
        exception->utf8 = jni_text_dup(message ? message : "");
    }

    return JNI_PENDING_EXCEPTION ? JNI_OK : JNI_ENOMEM;
}

jthrowable jni_entry_exception_occurred(JNIEnv* env)
{
    (void)env;
    return JNI_PENDING_EXCEPTION;
}

void jni_entry_exception_describe(JNIEnv* env)
{
    (void)env;
    jni_obj_t* exception = jni_obj_get(JNI_PENDING_EXCEPTION);
    if(!exception) {
        return;
    }

    const char* name = JEX_THROWABLE;
    if(exception->clazz && exception->clazz->name) {
        name = exception->clazz->name;
    }

    const char* message = exception->utf8 ? exception->utf8 : "";
    log_w(TAG, "JNI exception: %s: %s", name, message);
    JNI_PENDING_EXCEPTION = NULL;
}

void jni_entry_exception_clear(JNIEnv* env)
{
    (void)env;
    JNI_PENDING_EXCEPTION = NULL;
}

jboolean jni_entry_exception_check(JNIEnv* env)
{
    (void)env;
    return JNI_PENDING_EXCEPTION ? JNI_TRUE : JNI_FALSE;
}

void jni_entry_fatal_error(JNIEnv* env, const char* message)
{
    (void)env;
    log_f(TAG, "JNI FatalError: %s", message ? message : "(null)");
}
