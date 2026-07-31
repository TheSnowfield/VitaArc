#ifndef VITAARC_JNI_RUNTIME_OBJECT_H
#define VITAARC_JNI_RUNTIME_OBJECT_H

#include <stdint.h>
#include "../runtime.h"
#include "../thread.h"
#include "../references.h"
#include "../names.h"

/* Private VM storage. All operations require the runtime lock, except the
 * documented public wrappers that acquire it. Metadata lives until teardown;
 * ordinary objects are owned by the tracing heap, never by raw C pointers. */
#define JNI_KIND_MAGIC 0x4A4E4931u

typedef enum jni_kind_t {
    JNI_KIND_OBJECT,
    JNI_KIND_CLASS,
    JNI_KIND_STRING,
    JNI_KIND_METHOD,
    JNI_KIND_NATIVE,
    JNI_KIND_FIELD,
    JNI_KIND_ARRAY,
    JNI_KIND_DIRECT_BUFFER
} jni_kind_t;

typedef struct jni_slot {
    jfieldID field;
    jvalue value;
    struct jni_slot* next;
} jni_slot_t;

typedef struct jni_obj_t {
    /* Heap identity and linkage. */
    uint32_t magic;
    jni_kind_t kind;
    struct jni_obj_t* next;

    /* Class and member metadata. */
    struct jni_obj_t* clazz;
    char* name;
    char* signature;

    /* String and array payloads. */
    char* utf8;
    jchar* utf16;
    jsize length;
    size_t stride;
    char type;
    struct jni_obj_t* elem;
    void* data;

    /* Registered native and inherited declaration. */
    void* proc;
    struct jni_obj_t* decl;

    /* Reentrant monitor ownership. */
    jni_thread_t* owner;
    uint32_t depth;
    uint32_t waiters;

    /* Tracing state and field storage. */
    bool mark;
    struct jni_obj_t* gray;
    jni_slot_t* fields;
    jboolean is_static;
} jni_obj_t;

#define JNI_PENDING_EXCEPTION (jni_thread_current()->exception)

char* jni_text_dup(const char* text);

/**
 * @brief Allocate a VM-owned node in the tracing heap.
 * @pre The runtime lock is held.
 * @return A zeroed node, or NULL on allocation failure.
 */
jni_obj_t* jni_heap_new(jni_kind_t kind);

/**
 * @brief Resolve a JNI handle or an internal node while holding the runtime lock.
 */
jni_obj_t* jni_obj_get(const void* reference);

/**
 * @brief Borrow the heap list while holding the runtime lock.
 */
jni_obj_t* jni_heap_head(void);

#endif
