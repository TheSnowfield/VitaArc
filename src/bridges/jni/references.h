#ifndef __BRIDGES_JNI_REFERENCES_H
#define __BRIDGES_JNI_REFERENCES_H

#include "thread.h"

struct jni_obj_t;
typedef void (*jni_mark_object_t)(struct jni_obj_t* object);

struct jni_obj_t* jni_refs_resolve(const void* reference);
jobject jni_refs_new(struct jni_obj_t* object, jobjectRefType type);
void jni_refs_delete(jobject reference, jobjectRefType type);
jobjectRefType jni_refs_type(jobject reference);
bool jni_refs_is_new_local(jobject reference);
void jni_refs_clear_globals(void);
bool jni_refs_ensure(jni_thread_t* thread, jint capacity);
bool jni_refs_push(jni_thread_t* thread, jint capacity);
bool jni_refs_enter_native(jni_thread_t* thread, jint capacity);
void jni_refs_leave_native(jni_thread_t* thread);
void jni_refs_pop(jni_thread_t* thread);
void jni_refs_cleanup(jni_thread_t* thread);
void jni_refs_mark(jni_mark_object_t mark);
void jni_refs_clear_weak(bool (*alive)(struct jni_obj_t* object));
bool jni_refs_pin(struct jni_obj_t* object);
bool jni_refs_unpin(struct jni_obj_t* object);
void jni_refs_mark_pins(jni_mark_object_t mark);
size_t jni_refs_count(void);

#endif /* __BRIDGES_JNI_REFERENCES_H */
