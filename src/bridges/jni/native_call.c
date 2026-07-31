#include <stdint.h>
#include <string.h>

#include "native_call.h"

static bool __call_ok(void* proc, const jni_sig_t* sig, const jvalue* args, jvalue* result)
{
    if(!proc)
        return false;

    if(!sig)
        return false;

    if(!result)
        return false;

    if(sig->argc > JNI_MAX_CALL_ARGS)
        return false;

    if(!sig->argc)
        return true;

    return args != NULL;
}

#if defined(__vita__) && defined(__arm__)

_Static_assert(sizeof(void*) == 4 && sizeof(jlong) == 8 && sizeof(jdouble) == 8 && sizeof(jfloat) == 4,
    "JNI ARM call packing requires AAPCS32 widths");

/* Copy the stack tail before loading r0-r3. Both our saved frame and outgoing
 * argument area keep SP eight-byte aligned. BLX supports ARM and Thumb code.
 * JNI has no aggregate parameters/results; all scalar results fit r0-r1. */
void jni_native_arm_call(void* proc, const uint32_t* words, uint32_t count, uint32_t result[2]);
__asm__(".text\n"
        ".align 2\n"
        ".thumb\n"
        ".thumb_func\n"
        ".global jni_native_arm_call\n"
        ".type jni_native_arm_call, %function\n"
        "jni_native_arm_call:\n"
        ".fnstart\n"
        "push {r4, r5, r6, r7, r8, lr}\n"
        "mov r4, sp\n"
        "mov r5, r0\n"
        "mov r6, r1\n"
        "mov r7, r3\n"
        "subs r2, r2, #4\n"
        "lsls r2, r2, #2\n"
        "adds r3, r2, #7\n"
        "bic r3, r3, #7\n"
        "sub sp, sp, r3\n"
        "add r0, r6, #16\n"
        "mov r1, sp\n"
        "cmp r2, #0\n"
        "beq 2f\n"
        "1:\n"
        "ldr r3, [r0], #4\n"
        "str r3, [r1], #4\n"
        "subs r2, r2, #4\n"
        "bne 1b\n"
        "2:\n"
        "ldm r6, {r0, r1, r2, r3}\n"
        "blx r5\n"
        "str r0, [r7]\n"
        "str r1, [r7, #4]\n"
        "mov sp, r4\n"
        "pop {r4, r5, r6, r7, r8, pc}\n"
        ".cantunwind\n"
        ".fnend\n"
        ".size jni_native_arm_call, .-jni_native_arm_call\n");

bool jni_native_call(
    void* proc, JNIEnv* env, jobject receiver, const jni_sig_t* sig, const jvalue* arguments, jvalue* result)
{
    if(!__call_ok(proc, sig, arguments, result)) {
        return false;
    }

    uint32_t words[2 + 2 * JNI_MAX_CALL_ARGS] = {0};

    uint32_t count = 2;
    words[0] = (uintptr_t)env;
    words[1] = (uintptr_t)receiver;
    for(size_t index = 0; index < sig->argc; ++index) {
        const jvalue* argument = &arguments[index];
        char type = sig->args[index];
        if(type == 'J' || type == 'D') {
            count = (count + 1) & ~1u;
            memcpy(words + count, argument, 8);
            count += 2;
            continue;
        }

        uint32_t value;
        switch(type) {
            case 'Z':
                value = argument->z;
                break;

            case 'B':
                value = (uint32_t)argument->b;
                break;

            case 'C':
                value = argument->c;
                break;

            case 'S':
                value = (uint32_t)argument->s;
                break;

            case 'I':
                value = (uint32_t)argument->i;
                break;

            case 'F':
                memcpy(&value, &argument->f, 4);
                break;

            case 'L':
                value = (uintptr_t)argument->l;
                break;

            default:
                return false;
        }

        words[count++] = value;
    }

    if(!strchr("VZBCSIJFDL", sig->ret) || !sig->ret) {
        return false;
    }

    uint32_t returned[2] = {0};

    jni_native_arm_call(proc, words, count < 4 ? 4 : count, returned);
    memset(result, 0, sizeof(*result));
    switch(sig->ret) {
        case 'Z':
            result->z = (jboolean)returned[0];
            break;

        case 'B':
            result->b = (jbyte)returned[0];
            break;

        case 'C':
            result->c = (jchar)returned[0];
            break;

        case 'S':
            result->s = (jshort)returned[0];
            break;

        case 'I':
            result->i = (jint)returned[0];
            break;

        case 'J':
        case 'D':
            memcpy(result, returned, 8);
            break;

        case 'F':
            memcpy(&result->f, returned, 4);
            break;

        case 'L':
            result->l = (jobject)(uintptr_t)returned[0];
            break;
    }

    return true;
}

#else

#include <ffi.h>

static ffi_type* __ffi_type(char type)
{
    switch(type) {
        case 'V': return &ffi_type_void;
        case 'Z': return &ffi_type_uint8;
        case 'B': return &ffi_type_sint8;
        case 'C': return &ffi_type_uint16;
        case 'S': return &ffi_type_sint16;
        case 'I': return &ffi_type_sint32;
        case 'J': return &ffi_type_sint64;
        case 'F': return &ffi_type_float;
        case 'D': return &ffi_type_double;
        case 'L': return &ffi_type_pointer;
        default:  return NULL;
    }
}

bool jni_native_call(
    void* proc, JNIEnv* env, jobject receiver, const jni_sig_t* sig, const jvalue* arguments, jvalue* result)
{
    if(!__call_ok(proc, sig, arguments, result)) {
        return false;
    }

    ffi_type* types[JNI_MAX_CALL_ARGS + 2] = {&ffi_type_pointer, &ffi_type_pointer};

    void* values[JNI_MAX_CALL_ARGS + 2] = {&env, &receiver};

    for(size_t index = 0; index < sig->argc; ++index) {
        types[index + 2] = __ffi_type(sig->args[index]);
        if(!types[index + 2] || sig->args[index] == 'V') {
            return false;
        }

        values[index + 2] = (void*)&arguments[index];
    }

    ffi_cif call;
    ffi_type* returned_type = __ffi_type(sig->ret);
    if(!returned_type || ffi_prep_cif(&call, FFI_DEFAULT_ABI, sig->argc + 2, returned_type, types) != FFI_OK) {
        return false;
    }

    union {
        ffi_arg word;
        jfloat f;
        jdouble d;
        jlong j;
        void* l;
    } returned = {0};

    ffi_call(&call, FFI_FN(proc), &returned, values);
    memset(result, 0, sizeof(*result));
    switch(sig->ret) {
        case 'Z':
            result->z = (jboolean)returned.word;
            break;

        case 'B':
            result->b = (jbyte)returned.word;
            break;

        case 'C':
            result->c = (jchar)returned.word;
            break;

        case 'S':
            result->s = (jshort)returned.word;
            break;

        case 'I':
            result->i = (jint)returned.word;
            break;

        case 'J':
            result->j = returned.j;
            break;

        case 'F':
            result->f = returned.f;
            break;

        case 'D':
            result->d = returned.d;
            break;

        case 'L':
            result->l = returned.l;
            break;
    }

    return true;
}

#endif
