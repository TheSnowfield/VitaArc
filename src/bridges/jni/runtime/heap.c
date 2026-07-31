#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "entry.h"
#include "exception.h"

static jni_obj_t* __objects;

jni_obj_t* jni_heap_head(void)
{
    return __objects;
}

char* jni_text_dup(const char* text)
{
    if(!text) {
        return NULL;
    }

    size_t length = strlen(text) + 1;
    char* copy = malloc(length);
    if(copy) {
        memcpy(copy, text, length);
    }

    return copy;
}

jni_obj_t* jni_heap_new(jni_kind_t kind)
{
    jni_obj_t* object = calloc(1, sizeof(*object));
    if(!object) {
        return NULL;
    }

    object->magic = JNI_KIND_MAGIC;
    object->kind = kind;
    object->next = __objects;
    __objects = object;
    return object;
}

jni_obj_t* jni_obj_get(const void* reference)
{
    jni_obj_t* resolved = jni_refs_resolve(reference);
    if(resolved) {
        return resolved;
    }

    for(jni_obj_t* object = __objects; object; object = object->next) {
        if(object == reference) {
            return object;
        }
    }

    return NULL;
}

jint jni_entry_monitor_enter(JNIEnv* env, jobject object)
{
    jni_obj_t* value = jni_obj_get(object);
    if(!value) {
        jni_raise(env, JEX_NULL, "monitor object is null or invalid");
        return JNI_ERR;
    }

    jni_thread_t* thread = jni_thread_current();
    ++value->waiters;
    while(value->owner && value->owner != thread) {
        jni_thread_wait();
        if(!jni_thread_vm_alive()) {
            --value->waiters;
            return JNI_ERR;
        }
    }

    --value->waiters;
    if(value->depth == UINT32_MAX) {
        jni_raise(env, JEX_MONITOR, "monitor recursion overflow");
        return JNI_ERR;
    }

    value->owner = thread;
    ++value->depth;
    return JNI_OK;
}

jint jni_entry_monitor_exit(JNIEnv* env, jobject object)
{
    jni_obj_t* value = jni_obj_get(object);
    if(!value) {
        jni_raise(env, JEX_NULL, "monitor object is null or invalid");
        return JNI_ERR;
    }

    if(value->owner != jni_thread_current()) {
        jni_raise(env, JEX_MONITOR, "current thread does not own monitor");
        return JNI_ERR;
    }

    if(!--value->depth) {
        value->owner = NULL;
        jni_thread_notify();
    }

    return JNI_OK;
}

static jni_obj_t* __mark_stack;

static void __mark_object(jni_obj_t* object)
{
    if(!object || object->mark) {
        return;
    }

    object->mark = true;
    object->gray = __mark_stack;
    __mark_stack = object;
}

static bool __object_alive(jni_obj_t* object)
{
    return object->mark;
}

static void __mark_roots(void)
{
    for(jni_obj_t* object = __objects; object; object = object->next) {
        object->mark = false;
    }

    for(jni_obj_t* object = __objects; object; object = object->next) {
        if(object->kind == JNI_KIND_CLASS || object->kind == JNI_KIND_METHOD || object->kind == JNI_KIND_FIELD ||
            object->kind == JNI_KIND_NATIVE || object->owner || object->waiters) {
            __mark_object(object);
        }
    }

    for(jni_thread_t* thread = jni_thread_list(); thread; thread = thread->next) {
        __mark_object((jni_obj_t*)thread->exception);
        for(jni_native_roots_t* roots = thread->native_roots; roots; roots = roots->parent) {
            for(size_t index = 0; index < roots->count; ++index) {
                __mark_object(roots->objects[index]);
            }
        }
    }

    jni_refs_mark(__mark_object);
    jni_refs_mark_pins(__mark_object);
}

static void __mark_children(jni_obj_t* object)
{
    __mark_object(object->clazz);
    __mark_object(object->elem);
    for(jni_slot_t* field = object->fields; field; field = field->next) {
        jni_obj_t* member = (jni_obj_t*)field->field;
        if(member->signature[0] == 'L' || member->signature[0] == '[') {
            __mark_object((jni_obj_t*)field->value.l);
        }
    }

    if(object->kind == JNI_KIND_ARRAY && object->type == 'L') {
        for(jsize index = 0; index < object->length; ++index) {
            __mark_object(((jni_obj_t**)object->data)[index]);
        }
    }
}

static void __free_object(jni_obj_t* object)
{
    while(object->fields) {
        jni_slot_t* next = object->fields->next;
        free(object->fields);
        object->fields = next;
    }

    free(object->name);
    free(object->signature);
    free(object->utf8);
    free(object->utf16);
    if(object->kind == JNI_KIND_ARRAY) {
        free(object->data);
    }

    free(object);
}

void jni_runtime_vm_cleanup(void)
{
    jni_refs_clear_globals();
    while(__objects) {
        jni_obj_t* next = __objects->next;
        __free_object(__objects);
        __objects = next;
    }

    __mark_stack = NULL;
}

void jni_runtime_collect(void)
{
    __mark_roots();
    while(__mark_stack) {
        jni_obj_t* object = __mark_stack;
        __mark_stack = object->gray;
        __mark_children(object);
    }

    jni_refs_clear_weak(__object_alive);
    jni_obj_t** cursor = &__objects;
    while(*cursor) {
        jni_obj_t* object = *cursor;
        if(object->mark) {
            cursor = &object->next;
        } else {
            *cursor = object->next;
            __free_object(object);
        }
    }
}

void jni_runtime_get_stats(jni_runtime_stats_t* stats)
{
    jni_thread_lock();
    stats->objects = 0;
    stats->references = jni_refs_count();
    for(jni_obj_t* object = __objects; object; object = object->next) {
        ++stats->objects;
    }

    jni_thread_unlock();
}

void jni_runtime_thread_cleanup(jni_thread_t* thread)
{
    thread->exception = NULL;
    thread->native_roots = NULL;
    jni_refs_cleanup(thread);
    for(jni_obj_t* object = __objects; object; object = object->next) {
        if(object->owner == thread) {
            object->owner = NULL;
            object->depth = 0;
        }
    }

    jni_runtime_collect();
    jni_thread_notify();
}
