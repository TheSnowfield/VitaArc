#include <stdint.h>
#include <stdlib.h>

#include "references.h"

typedef struct jni_reference {
    struct jni_obj_t* object;
    jobjectRefType type;
    jni_thread_t* owner;
    uint64_t call_serial;
    struct jni_reference* next;
} jni_reference_t;

typedef struct jni_reference_block {
    size_t capacity;
    struct jni_reference_block* next;
    jni_reference_t references[];
} jni_reference_block_t;

typedef struct jni_frame {
    jni_reference_block_t* blocks;
    struct jni_frame* parent;
    bool native_boundary;
} jni_frame_t;

typedef struct jni_pin {
    struct jni_obj_t* object;
    size_t count;
    struct jni_pin* next;
} jni_pin_t;

static jni_reference_t* __references;

static jni_reference_t* __find(jobject value)
{
    for(jni_reference_t* reference = __references; reference; reference = reference->next) {
        if(reference == value) {
            return reference;
        }
    }

    return NULL;
}

static bool __visible(jni_reference_t* reference)
{
    return !reference->owner || reference->owner == jni_thread_current();
}

struct jni_obj_t* jni_refs_resolve(const void* value)
{
    jni_reference_t* reference = __find((jobject)value);
    return reference && __visible(reference) ? reference->object : NULL;
}

jobjectRefType jni_refs_type(jobject value)
{
    jni_reference_t* reference = __find(value);
    return reference && __visible(reference) ? reference->type : JNIInvalidRefType;
}

bool jni_refs_is_new_local(jobject value)
{
    jni_reference_t* reference = __find(value);
    jni_thread_t* thread = jni_thread_current();
    return reference && reference->owner == thread && reference->type == JNILocalRefType &&
        reference->call_serial == thread->call_serial;
}

static bool __reserve(jni_frame_t* frame, size_t capacity)
{
    if(capacity > (SIZE_MAX - sizeof(jni_reference_block_t)) / sizeof(jni_reference_t)) {
        return false;
    }

    jni_reference_block_t* block = calloc(1, sizeof(*block) + capacity * sizeof(jni_reference_t));
    if(!block) {
        return false;
    }

    block->capacity = capacity;
    block->next = frame->blocks;
    frame->blocks = block;
    return true;
}

bool jni_refs_push(jni_thread_t* thread, jint capacity)
{
    if(capacity < 0) {
        return false;
    }

    jni_frame_t* frame = calloc(1, sizeof(*frame));
    if(!frame) {
        return false;
    }

    if(!__reserve(frame, capacity ? (size_t)capacity : 1)) {
        free(frame);
        return false;
    }

    frame->parent = thread->frame;
    thread->frame = frame;
    return true;
}

bool jni_refs_ensure(jni_thread_t* thread, jint capacity)
{
    if(capacity < 0) {
        return false;
    }

    if(!thread->frame) {
        return jni_refs_push(thread, capacity > 32 ? capacity : 32);
    }

    size_t available = 0;
    for(jni_reference_block_t* block = thread->frame->blocks; block; block = block->next) {
        for(size_t index = 0; index < block->capacity; ++index) {
            available += block->references[index].type == JNIInvalidRefType;
        }
    }

    return available >= (size_t)capacity || __reserve(thread->frame, (size_t)capacity - available);
}

static jni_reference_t* __local_slot(jni_thread_t* thread)
{
    if(!jni_refs_ensure(thread, 1)) {
        return NULL;
    }

    for(jni_reference_block_t* block = thread->frame->blocks; block; block = block->next) {
        for(size_t index = 0; index < block->capacity; ++index) {
            if(block->references[index].type == JNIInvalidRefType) {
                return &block->references[index];
            }
        }
    }

    return NULL;
}

jobject jni_refs_new(struct jni_obj_t* object, jobjectRefType type)
{
    if(!object) {
        return NULL;
    }

    jni_thread_t* owner = type == JNILocalRefType ? jni_thread_current() : NULL;
    jni_reference_t* reference = owner ? __local_slot(owner) : calloc(1, sizeof(*reference));
    if(!reference) {
        return NULL;
    }

    reference->object = object;
    reference->owner = owner;
    reference->type = type;
    reference->call_serial = owner ? owner->call_serial : 0;
    reference->next = __references;
    __references = reference;
    return (jobject)reference;
}

static void __unlink(jni_reference_t* reference)
{
    jni_reference_t** cursor = &__references;
    while(*cursor && *cursor != reference) {
        cursor = &(*cursor)->next;
    }

    if(*cursor) {
        *cursor = reference->next;
    }

    reference->object = NULL;
    reference->type = JNIInvalidRefType;
}

void jni_refs_delete(jobject value, jobjectRefType type)
{
    jni_reference_t* reference = __find(value);
    if(!reference || reference->type != type || !__visible(reference)) {
        return;
    }

    __unlink(reference);
    if(type != JNILocalRefType) {
        free(reference);
    }
}

static void __pop_frame(jni_thread_t* thread)
{
    jni_frame_t* frame = thread->frame;
    if(!frame) {
        return;
    }

    thread->frame = frame->parent;
    jni_reference_block_t* block = frame->blocks;
    while(block) {
        jni_reference_block_t* next = block->next;
        for(size_t index = 0; index < block->capacity; ++index) {
            if(block->references[index].type != JNIInvalidRefType) {
                __unlink(&block->references[index]);
            }
        }

        free(block);
        block = next;
    }

    free(frame);
}

void jni_refs_pop(jni_thread_t* thread)
{
    if(thread->frame && !thread->frame->native_boundary) {
        __pop_frame(thread);
    }
}

bool jni_refs_enter_native(jni_thread_t* thread, jint capacity)
{
    if(!jni_refs_push(thread, capacity)) {
        return false;
    }

    thread->frame->native_boundary = true;
    return true;
}

void jni_refs_leave_native(jni_thread_t* thread)
{
    while(thread->frame && !thread->frame->native_boundary) {
        __pop_frame(thread);
    }

    __pop_frame(thread);
}

void jni_refs_clear_globals(void)
{
    jni_reference_t** cursor = &__references;
    while(*cursor) {
        jni_reference_t* reference = *cursor;
        if(reference->owner) {
            cursor = &reference->next;
        } else {
            *cursor = reference->next;
            free(reference);
        }
    }
}

void jni_refs_cleanup(jni_thread_t* thread)
{
    while(thread->frame) {
        __pop_frame(thread);
    }

    while(thread->pins) {
        jni_pin_t* next = thread->pins->next;
        free(thread->pins);
        thread->pins = next;
    }
}

void jni_refs_mark(jni_mark_object_t mark)
{
    for(jni_reference_t* reference = __references; reference; reference = reference->next) {
        if(reference->type != JNIWeakGlobalRefType) {
            mark(reference->object);
        }
    }
}

void jni_refs_clear_weak(bool (*alive)(struct jni_obj_t* object))
{
    for(jni_reference_t* reference = __references; reference; reference = reference->next) {
        if(reference->type == JNIWeakGlobalRefType && reference->object && !alive(reference->object)) {
            reference->object = NULL;
        }
    }
}

bool jni_refs_pin(struct jni_obj_t* object)
{
    jni_thread_t* thread = jni_thread_current();
    for(jni_pin_t* pin = thread->pins; pin; pin = pin->next) {
        if(pin->object == object) {
            ++pin->count;
            return true;
        }
    }

    jni_pin_t* pin = calloc(1, sizeof(*pin));
    if(!pin) {
        return false;
    }

    pin->object = object;
    pin->count = 1;
    pin->next = thread->pins;
    thread->pins = pin;
    return true;
}

bool jni_refs_unpin(struct jni_obj_t* object)
{
    jni_pin_t** cursor = &jni_thread_current()->pins;
    while(*cursor) {
        jni_pin_t* pin = *cursor;
        if(pin->object == object) {
            if(!--pin->count) {
                *cursor = pin->next;
                free(pin);
            }

            return true;
        }

        cursor = &pin->next;
    }

    return false;
}

void jni_refs_mark_pins(jni_mark_object_t mark)
{
    for(jni_thread_t* thread = jni_thread_list(); thread; thread = thread->next) {
        for(jni_pin_t* pin = thread->pins; pin; pin = pin->next) {
            mark(pin->object);
        }
    }
}

size_t jni_refs_count(void)
{
    size_t count = 0;
    for(jni_reference_t* reference = __references; reference; reference = reference->next) {
        ++count;
    }

    return count;
}
