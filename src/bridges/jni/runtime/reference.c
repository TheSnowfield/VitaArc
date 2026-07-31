
#include "object.h"
#include "entry.h"
#include "exception.h"
#include "reference.h"

jobject jni_local(JNIEnv* env, jobject value)
{
    if(jni_refs_is_new_local(value)) {
        return value;
    }

    jni_obj_t* object = jni_obj_get(value);
    jobject reference = jni_refs_new(object, JNILocalRefType);
    if(object && !reference) {
        jni_raise(env, JEX_MEMORY, "JNI local reference allocation failed");
    }

    return reference;
}

jint jni_entry_push_local_frame(JNIEnv* env, jint capacity)
{
    if(capacity < 0) {
        jni_raise(env, JEX_ARG, "negative local frame capacity");
        return JNI_ERR;
    }

    jni_thread_t* thread = jni_thread_current();
    if(!jni_refs_ensure(thread, 0) || !jni_refs_push(thread, capacity)) {
        jni_raise(env, JEX_MEMORY, "cannot reserve local frame");
        return JNI_ENOMEM;
    }

    return JNI_OK;
}

jobject jni_entry_pop_local_frame(JNIEnv* env, jobject result)
{
    jni_obj_t* object = jni_obj_get(result);
    jni_refs_pop(jni_thread_current());
    return jni_local(env, (jobject)object);
}

jobject jni_entry_new_global_ref(JNIEnv* env, jobject object)
{
    jni_obj_t* value = jni_obj_get(object);
    jobject reference = jni_refs_new(value, JNIGlobalRefType);
    if(value && !reference) {
        jni_raise(env, JEX_MEMORY, "cannot allocate global reference");
    }

    return reference;
}

void jni_entry_delete_global_ref(JNIEnv* env, jobject object)
{
    (void)env;
    jni_refs_delete(object, JNIGlobalRefType);
}

void jni_entry_delete_local_ref(JNIEnv* env, jobject object)
{
    (void)env;
    jni_refs_delete(object, JNILocalRefType);
}

jboolean jni_entry_is_same_object(JNIEnv* env, jobject first, jobject second)
{
    (void)env;
    return jni_obj_get(first) == jni_obj_get(second);
}

jobject jni_entry_new_local_ref(JNIEnv* env, jobject object)
{
    jni_obj_t* value = jni_obj_get(object);
    jobject reference = jni_refs_new(value, JNILocalRefType);
    if(value && !reference) {
        jni_raise(env, JEX_MEMORY, "cannot allocate local reference");
    }

    return reference;
}

jint jni_entry_ensure_local_capacity(JNIEnv* env, jint capacity)
{
    if(capacity < 0) {
        jni_raise(env, JEX_ARG, "negative local capacity");
        return JNI_ERR;
    }

    if(!jni_refs_ensure(jni_thread_current(), capacity)) {
        jni_raise(env, JEX_MEMORY, "cannot reserve local references");
        return JNI_ENOMEM;
    }

    return JNI_OK;
}

jweak jni_entry_new_weak_global_ref(JNIEnv* env, jobject object)
{
    jni_obj_t* value = jni_obj_get(object);
    jweak reference = jni_refs_new(value, JNIWeakGlobalRefType);
    if(value && !reference) {
        jni_raise(env, JEX_MEMORY, "cannot allocate weak reference");
    }

    return reference;
}

void jni_entry_delete_weak_global_ref(JNIEnv* env, jweak object)
{
    (void)env;
    jni_refs_delete(object, JNIWeakGlobalRefType);
}

jobjectRefType jni_entry_get_object_ref_type(JNIEnv* env, jobject object)
{
    (void)env;
    return jni_refs_type(object);
}
