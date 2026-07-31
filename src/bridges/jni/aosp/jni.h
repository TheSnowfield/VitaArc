/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * JNI specification, as defined by Sun:
 * http://java.sun.com/javase/6/docs/technotes/guides/jni/spec/jniTOC.html
 *
 * Everything here is expected to be VM-neutral.
 */

#ifndef __BRIDGES_JNI_AOSP_JNI_H
#define __BRIDGES_JNI_AOSP_JNI_H

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
/* Android ARM uses the base PCS even when VitaArc is built with hard-float.
 * Keep floating-point arguments and returns consistent in both C and C++. */
#include "../../abi/calling.h"

/* Primitive types that match up with Java equivalents. */
typedef uint8_t  jboolean; /* unsigned 8 bits */
typedef int8_t   jbyte;    /* signed 8 bits */
typedef uint16_t jchar;    /* unsigned 16 bits */
typedef int16_t  jshort;   /* signed 16 bits */
typedef int32_t  jint;     /* signed 32 bits */
typedef int64_t  jlong;    /* signed 64 bits */
typedef float    jfloat;   /* 32-bit IEEE 754 */
typedef double   jdouble;  /* 64-bit IEEE 754 */

/* "cardinal indices and sizes" */
typedef jint     jsize;

#ifdef __cplusplus
/*
 * Reference types, in C++
 */
class _jobject {};
class _jclass : public _jobject {};
class _jstring : public _jobject {};
class _jarray : public _jobject {};
class _jobjectArray : public _jarray {};
class _jbooleanArray : public _jarray {};
class _jbyteArray : public _jarray {};
class _jcharArray : public _jarray {};
class _jshortArray : public _jarray {};
class _jintArray : public _jarray {};
class _jlongArray : public _jarray {};
class _jfloatArray : public _jarray {};
class _jdoubleArray : public _jarray {};
class _jthrowable : public _jobject {};

typedef _jobject*       jobject;
typedef _jclass*        jclass;
typedef _jstring*       jstring;
typedef _jarray*        jarray;
typedef _jobjectArray*  jobjectArray;
typedef _jbooleanArray* jbooleanArray;
typedef _jbyteArray*    jbyteArray;
typedef _jcharArray*    jcharArray;
typedef _jshortArray*   jshortArray;
typedef _jintArray*     jintArray;
typedef _jlongArray*    jlongArray;
typedef _jfloatArray*   jfloatArray;
typedef _jdoubleArray*  jdoubleArray;
typedef _jthrowable*    jthrowable;
typedef _jobject*       jweak;


#else /* not __cplusplus */

/*
 * Reference types, in C.
 */
typedef void*           jobject;
typedef jobject         jclass;
typedef jobject         jstring;
typedef jobject         jarray;
typedef jarray          jobjectArray;
typedef jarray          jbooleanArray;
typedef jarray          jbyteArray;
typedef jarray          jcharArray;
typedef jarray          jshortArray;
typedef jarray          jintArray;
typedef jarray          jlongArray;
typedef jarray          jfloatArray;
typedef jarray          jdoubleArray;
typedef jobject         jthrowable;
typedef jobject         jweak;

#endif /* not __cplusplus */

struct _jfieldID;                       /* opaque structure */
typedef struct _jfieldID* jfieldID;     /* field IDs */

struct _jmethodID;                      /* opaque structure */
typedef struct _jmethodID* jmethodID;   /* method IDs */

struct JNIInvokeInterface;

typedef union jvalue {
    jboolean    z;
    jbyte       b;
    jchar       c;
    jshort      s;
    jint        i;
    jlong       j;
    jfloat      f;
    jdouble     d;
    jobject     l;
} jvalue;

typedef enum jobjectRefType {
    JNIInvalidRefType = 0,
    JNILocalRefType = 1,
    JNIGlobalRefType = 2,
    JNIWeakGlobalRefType = 3
} jobjectRefType;

typedef struct {
    const char* name;
    const char* signature;
    void*       fnPtr;
} JNINativeMethod;

struct _JNIEnv;
struct _JavaVM;
typedef const struct JNINativeInterface* C_JNIEnv;

#if defined(__cplusplus)
typedef _JNIEnv JNIEnv;
typedef _JavaVM JavaVM;
#else
typedef const struct JNINativeInterface* JNIEnv;
typedef const struct JNIInvokeInterface* JavaVM;
#endif

/*
 * Table of interface function pointers.
 */
struct JNINativeInterface {
    void*       reserved0;
    void*       reserved1;
    void*       reserved2;
    void*       reserved3;

    jint        VITAARC_SOFTFP (*GetVersion)(JNIEnv *);

    jclass      VITAARC_SOFTFP (*DefineClass)(JNIEnv*, const char*, jobject, const jbyte*,
                        jsize);
    jclass      VITAARC_SOFTFP (*FindClass)(JNIEnv*, const char*);

    jmethodID   VITAARC_SOFTFP (*FromReflectedMethod)(JNIEnv*, jobject);
    jfieldID    VITAARC_SOFTFP (*FromReflectedField)(JNIEnv*, jobject);
    /* spec doesn't show jboolean parameter */
    jobject     VITAARC_SOFTFP (*ToReflectedMethod)(JNIEnv*, jclass, jmethodID, jboolean);

    jclass      VITAARC_SOFTFP (*GetSuperclass)(JNIEnv*, jclass);
    jboolean    VITAARC_SOFTFP (*IsAssignableFrom)(JNIEnv*, jclass, jclass);

    /* spec doesn't show jboolean parameter */
    jobject     VITAARC_SOFTFP (*ToReflectedField)(JNIEnv*, jclass, jfieldID, jboolean);

    jint        VITAARC_SOFTFP (*Throw)(JNIEnv*, jthrowable);
    jint        VITAARC_SOFTFP (*ThrowNew)(JNIEnv *, jclass, const char *);
    jthrowable  VITAARC_SOFTFP (*ExceptionOccurred)(JNIEnv*);
    void        VITAARC_SOFTFP (*ExceptionDescribe)(JNIEnv*);
    void        VITAARC_SOFTFP (*ExceptionClear)(JNIEnv*);
    void        VITAARC_SOFTFP (*FatalError)(JNIEnv*, const char*);

    jint        VITAARC_SOFTFP (*PushLocalFrame)(JNIEnv*, jint);
    jobject     VITAARC_SOFTFP (*PopLocalFrame)(JNIEnv*, jobject);

    jobject     VITAARC_SOFTFP (*NewGlobalRef)(JNIEnv*, jobject);
    void        VITAARC_SOFTFP (*DeleteGlobalRef)(JNIEnv*, jobject);
    void        VITAARC_SOFTFP (*DeleteLocalRef)(JNIEnv*, jobject);
    jboolean    VITAARC_SOFTFP (*IsSameObject)(JNIEnv*, jobject, jobject);

    jobject     VITAARC_SOFTFP (*NewLocalRef)(JNIEnv*, jobject);
    jint        VITAARC_SOFTFP (*EnsureLocalCapacity)(JNIEnv*, jint);

    jobject     VITAARC_SOFTFP (*AllocObject)(JNIEnv*, jclass);
    jobject     VITAARC_SOFTFP (*NewObject)(JNIEnv*, jclass, jmethodID, ...);
    jobject     VITAARC_SOFTFP (*NewObjectV)(JNIEnv*, jclass, jmethodID, va_list);
    jobject     VITAARC_SOFTFP (*NewObjectA)(JNIEnv*, jclass, jmethodID, const jvalue*);

    jclass      VITAARC_SOFTFP (*GetObjectClass)(JNIEnv*, jobject);
    jboolean    VITAARC_SOFTFP (*IsInstanceOf)(JNIEnv*, jobject, jclass);
    jmethodID   VITAARC_SOFTFP (*GetMethodID)(JNIEnv*, jclass, const char*, const char*);

    jobject     VITAARC_SOFTFP (*CallObjectMethod)(JNIEnv*, jobject, jmethodID, ...);
    jobject     VITAARC_SOFTFP (*CallObjectMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jobject     VITAARC_SOFTFP (*CallObjectMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
    jboolean    VITAARC_SOFTFP (*CallBooleanMethod)(JNIEnv*, jobject, jmethodID, ...);
    jboolean    VITAARC_SOFTFP (*CallBooleanMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jboolean    VITAARC_SOFTFP (*CallBooleanMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
    jbyte       VITAARC_SOFTFP (*CallByteMethod)(JNIEnv*, jobject, jmethodID, ...);
    jbyte       VITAARC_SOFTFP (*CallByteMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jbyte       VITAARC_SOFTFP (*CallByteMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
    jchar       VITAARC_SOFTFP (*CallCharMethod)(JNIEnv*, jobject, jmethodID, ...);
    jchar       VITAARC_SOFTFP (*CallCharMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jchar       VITAARC_SOFTFP (*CallCharMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
    jshort      VITAARC_SOFTFP (*CallShortMethod)(JNIEnv*, jobject, jmethodID, ...);
    jshort      VITAARC_SOFTFP (*CallShortMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jshort      VITAARC_SOFTFP (*CallShortMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
    jint        VITAARC_SOFTFP (*CallIntMethod)(JNIEnv*, jobject, jmethodID, ...);
    jint        VITAARC_SOFTFP (*CallIntMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jint        VITAARC_SOFTFP (*CallIntMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
    jlong       VITAARC_SOFTFP (*CallLongMethod)(JNIEnv*, jobject, jmethodID, ...);
    jlong       VITAARC_SOFTFP (*CallLongMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jlong       VITAARC_SOFTFP (*CallLongMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
    jfloat      VITAARC_SOFTFP (*CallFloatMethod)(JNIEnv*, jobject, jmethodID, ...);
    jfloat      VITAARC_SOFTFP (*CallFloatMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jfloat      VITAARC_SOFTFP (*CallFloatMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
    jdouble     VITAARC_SOFTFP (*CallDoubleMethod)(JNIEnv*, jobject, jmethodID, ...);
    jdouble     VITAARC_SOFTFP (*CallDoubleMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jdouble     VITAARC_SOFTFP (*CallDoubleMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);
    void        VITAARC_SOFTFP (*CallVoidMethod)(JNIEnv*, jobject, jmethodID, ...);
    void        VITAARC_SOFTFP (*CallVoidMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    void        VITAARC_SOFTFP (*CallVoidMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);

    jobject     VITAARC_SOFTFP (*CallNonvirtualObjectMethod)(JNIEnv*, jobject, jclass,
                        jmethodID, ...);
    jobject     VITAARC_SOFTFP (*CallNonvirtualObjectMethodV)(JNIEnv*, jobject, jclass,
                        jmethodID, va_list);
    jobject     VITAARC_SOFTFP (*CallNonvirtualObjectMethodA)(JNIEnv*, jobject, jclass,
                        jmethodID, const jvalue*);
    jboolean    VITAARC_SOFTFP (*CallNonvirtualBooleanMethod)(JNIEnv*, jobject, jclass,
                        jmethodID, ...);
    jboolean    VITAARC_SOFTFP (*CallNonvirtualBooleanMethodV)(JNIEnv*, jobject, jclass,
                         jmethodID, va_list);
    jboolean    VITAARC_SOFTFP (*CallNonvirtualBooleanMethodA)(JNIEnv*, jobject, jclass,
                         jmethodID, const jvalue*);
    jbyte       VITAARC_SOFTFP (*CallNonvirtualByteMethod)(JNIEnv*, jobject, jclass,
                        jmethodID, ...);
    jbyte       VITAARC_SOFTFP (*CallNonvirtualByteMethodV)(JNIEnv*, jobject, jclass,
                        jmethodID, va_list);
    jbyte       VITAARC_SOFTFP (*CallNonvirtualByteMethodA)(JNIEnv*, jobject, jclass,
                        jmethodID, const jvalue*);
    jchar       VITAARC_SOFTFP (*CallNonvirtualCharMethod)(JNIEnv*, jobject, jclass,
                        jmethodID, ...);
    jchar       VITAARC_SOFTFP (*CallNonvirtualCharMethodV)(JNIEnv*, jobject, jclass,
                        jmethodID, va_list);
    jchar       VITAARC_SOFTFP (*CallNonvirtualCharMethodA)(JNIEnv*, jobject, jclass,
                        jmethodID, const jvalue*);
    jshort      VITAARC_SOFTFP (*CallNonvirtualShortMethod)(JNIEnv*, jobject, jclass,
                        jmethodID, ...);
    jshort      VITAARC_SOFTFP (*CallNonvirtualShortMethodV)(JNIEnv*, jobject, jclass,
                        jmethodID, va_list);
    jshort      VITAARC_SOFTFP (*CallNonvirtualShortMethodA)(JNIEnv*, jobject, jclass,
                        jmethodID, const jvalue*);
    jint        VITAARC_SOFTFP (*CallNonvirtualIntMethod)(JNIEnv*, jobject, jclass,
                        jmethodID, ...);
    jint        VITAARC_SOFTFP (*CallNonvirtualIntMethodV)(JNIEnv*, jobject, jclass,
                        jmethodID, va_list);
    jint        VITAARC_SOFTFP (*CallNonvirtualIntMethodA)(JNIEnv*, jobject, jclass,
                        jmethodID, const jvalue*);
    jlong       VITAARC_SOFTFP (*CallNonvirtualLongMethod)(JNIEnv*, jobject, jclass,
                        jmethodID, ...);
    jlong       VITAARC_SOFTFP (*CallNonvirtualLongMethodV)(JNIEnv*, jobject, jclass,
                        jmethodID, va_list);
    jlong       VITAARC_SOFTFP (*CallNonvirtualLongMethodA)(JNIEnv*, jobject, jclass,
                        jmethodID, const jvalue*);
    jfloat      VITAARC_SOFTFP (*CallNonvirtualFloatMethod)(JNIEnv*, jobject, jclass,
                        jmethodID, ...);
    jfloat      VITAARC_SOFTFP (*CallNonvirtualFloatMethodV)(JNIEnv*, jobject, jclass,
                        jmethodID, va_list);
    jfloat      VITAARC_SOFTFP (*CallNonvirtualFloatMethodA)(JNIEnv*, jobject, jclass,
                        jmethodID, const jvalue*);
    jdouble     VITAARC_SOFTFP (*CallNonvirtualDoubleMethod)(JNIEnv*, jobject, jclass,
                        jmethodID, ...);
    jdouble     VITAARC_SOFTFP (*CallNonvirtualDoubleMethodV)(JNIEnv*, jobject, jclass,
                        jmethodID, va_list);
    jdouble     VITAARC_SOFTFP (*CallNonvirtualDoubleMethodA)(JNIEnv*, jobject, jclass,
                        jmethodID, const jvalue*);
    void        VITAARC_SOFTFP (*CallNonvirtualVoidMethod)(JNIEnv*, jobject, jclass,
                        jmethodID, ...);
    void        VITAARC_SOFTFP (*CallNonvirtualVoidMethodV)(JNIEnv*, jobject, jclass,
                        jmethodID, va_list);
    void        VITAARC_SOFTFP (*CallNonvirtualVoidMethodA)(JNIEnv*, jobject, jclass,
                        jmethodID, const jvalue*);

    jfieldID    VITAARC_SOFTFP (*GetFieldID)(JNIEnv*, jclass, const char*, const char*);

    jobject     VITAARC_SOFTFP (*GetObjectField)(JNIEnv*, jobject, jfieldID);
    jboolean    VITAARC_SOFTFP (*GetBooleanField)(JNIEnv*, jobject, jfieldID);
    jbyte       VITAARC_SOFTFP (*GetByteField)(JNIEnv*, jobject, jfieldID);
    jchar       VITAARC_SOFTFP (*GetCharField)(JNIEnv*, jobject, jfieldID);
    jshort      VITAARC_SOFTFP (*GetShortField)(JNIEnv*, jobject, jfieldID);
    jint        VITAARC_SOFTFP (*GetIntField)(JNIEnv*, jobject, jfieldID);
    jlong       VITAARC_SOFTFP (*GetLongField)(JNIEnv*, jobject, jfieldID);
    jfloat      VITAARC_SOFTFP (*GetFloatField)(JNIEnv*, jobject, jfieldID);
    jdouble     VITAARC_SOFTFP (*GetDoubleField)(JNIEnv*, jobject, jfieldID);

    void        VITAARC_SOFTFP (*SetObjectField)(JNIEnv*, jobject, jfieldID, jobject);
    void        VITAARC_SOFTFP (*SetBooleanField)(JNIEnv*, jobject, jfieldID, jboolean);
    void        VITAARC_SOFTFP (*SetByteField)(JNIEnv*, jobject, jfieldID, jbyte);
    void        VITAARC_SOFTFP (*SetCharField)(JNIEnv*, jobject, jfieldID, jchar);
    void        VITAARC_SOFTFP (*SetShortField)(JNIEnv*, jobject, jfieldID, jshort);
    void        VITAARC_SOFTFP (*SetIntField)(JNIEnv*, jobject, jfieldID, jint);
    void        VITAARC_SOFTFP (*SetLongField)(JNIEnv*, jobject, jfieldID, jlong);
    void        VITAARC_SOFTFP (*SetFloatField)(JNIEnv*, jobject, jfieldID, jfloat);
    void        VITAARC_SOFTFP (*SetDoubleField)(JNIEnv*, jobject, jfieldID, jdouble);

    jmethodID   VITAARC_SOFTFP (*GetStaticMethodID)(JNIEnv*, jclass, const char*, const char*);

    jobject     VITAARC_SOFTFP (*CallStaticObjectMethod)(JNIEnv*, jclass, jmethodID, ...);
    jobject     VITAARC_SOFTFP (*CallStaticObjectMethodV)(JNIEnv*, jclass, jmethodID, va_list);
    jobject     VITAARC_SOFTFP (*CallStaticObjectMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
    jboolean    VITAARC_SOFTFP (*CallStaticBooleanMethod)(JNIEnv*, jclass, jmethodID, ...);
    jboolean    VITAARC_SOFTFP (*CallStaticBooleanMethodV)(JNIEnv*, jclass, jmethodID,
                        va_list);
    jboolean    VITAARC_SOFTFP (*CallStaticBooleanMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
    jbyte       VITAARC_SOFTFP (*CallStaticByteMethod)(JNIEnv*, jclass, jmethodID, ...);
    jbyte       VITAARC_SOFTFP (*CallStaticByteMethodV)(JNIEnv*, jclass, jmethodID, va_list);
    jbyte       VITAARC_SOFTFP (*CallStaticByteMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
    jchar       VITAARC_SOFTFP (*CallStaticCharMethod)(JNIEnv*, jclass, jmethodID, ...);
    jchar       VITAARC_SOFTFP (*CallStaticCharMethodV)(JNIEnv*, jclass, jmethodID, va_list);
    jchar       VITAARC_SOFTFP (*CallStaticCharMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
    jshort      VITAARC_SOFTFP (*CallStaticShortMethod)(JNIEnv*, jclass, jmethodID, ...);
    jshort      VITAARC_SOFTFP (*CallStaticShortMethodV)(JNIEnv*, jclass, jmethodID, va_list);
    jshort      VITAARC_SOFTFP (*CallStaticShortMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
    jint        VITAARC_SOFTFP (*CallStaticIntMethod)(JNIEnv*, jclass, jmethodID, ...);
    jint        VITAARC_SOFTFP (*CallStaticIntMethodV)(JNIEnv*, jclass, jmethodID, va_list);
    jint        VITAARC_SOFTFP (*CallStaticIntMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
    jlong       VITAARC_SOFTFP (*CallStaticLongMethod)(JNIEnv*, jclass, jmethodID, ...);
    jlong       VITAARC_SOFTFP (*CallStaticLongMethodV)(JNIEnv*, jclass, jmethodID, va_list);
    jlong       VITAARC_SOFTFP (*CallStaticLongMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
    jfloat      VITAARC_SOFTFP (*CallStaticFloatMethod)(JNIEnv*, jclass, jmethodID, ...);
    jfloat      VITAARC_SOFTFP (*CallStaticFloatMethodV)(JNIEnv*, jclass, jmethodID, va_list);
    jfloat      VITAARC_SOFTFP (*CallStaticFloatMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
    jdouble     VITAARC_SOFTFP (*CallStaticDoubleMethod)(JNIEnv*, jclass, jmethodID, ...);
    jdouble     VITAARC_SOFTFP (*CallStaticDoubleMethodV)(JNIEnv*, jclass, jmethodID, va_list);
    jdouble     VITAARC_SOFTFP (*CallStaticDoubleMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);
    void        VITAARC_SOFTFP (*CallStaticVoidMethod)(JNIEnv*, jclass, jmethodID, ...);
    void        VITAARC_SOFTFP (*CallStaticVoidMethodV)(JNIEnv*, jclass, jmethodID, va_list);
    void        VITAARC_SOFTFP (*CallStaticVoidMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);

    jfieldID    VITAARC_SOFTFP (*GetStaticFieldID)(JNIEnv*, jclass, const char*,
                        const char*);

    jobject     VITAARC_SOFTFP (*GetStaticObjectField)(JNIEnv*, jclass, jfieldID);
    jboolean    VITAARC_SOFTFP (*GetStaticBooleanField)(JNIEnv*, jclass, jfieldID);
    jbyte       VITAARC_SOFTFP (*GetStaticByteField)(JNIEnv*, jclass, jfieldID);
    jchar       VITAARC_SOFTFP (*GetStaticCharField)(JNIEnv*, jclass, jfieldID);
    jshort      VITAARC_SOFTFP (*GetStaticShortField)(JNIEnv*, jclass, jfieldID);
    jint        VITAARC_SOFTFP (*GetStaticIntField)(JNIEnv*, jclass, jfieldID);
    jlong       VITAARC_SOFTFP (*GetStaticLongField)(JNIEnv*, jclass, jfieldID);
    jfloat      VITAARC_SOFTFP (*GetStaticFloatField)(JNIEnv*, jclass, jfieldID);
    jdouble     VITAARC_SOFTFP (*GetStaticDoubleField)(JNIEnv*, jclass, jfieldID);

    void        VITAARC_SOFTFP (*SetStaticObjectField)(JNIEnv*, jclass, jfieldID, jobject);
    void        VITAARC_SOFTFP (*SetStaticBooleanField)(JNIEnv*, jclass, jfieldID, jboolean);
    void        VITAARC_SOFTFP (*SetStaticByteField)(JNIEnv*, jclass, jfieldID, jbyte);
    void        VITAARC_SOFTFP (*SetStaticCharField)(JNIEnv*, jclass, jfieldID, jchar);
    void        VITAARC_SOFTFP (*SetStaticShortField)(JNIEnv*, jclass, jfieldID, jshort);
    void        VITAARC_SOFTFP (*SetStaticIntField)(JNIEnv*, jclass, jfieldID, jint);
    void        VITAARC_SOFTFP (*SetStaticLongField)(JNIEnv*, jclass, jfieldID, jlong);
    void        VITAARC_SOFTFP (*SetStaticFloatField)(JNIEnv*, jclass, jfieldID, jfloat);
    void        VITAARC_SOFTFP (*SetStaticDoubleField)(JNIEnv*, jclass, jfieldID, jdouble);

    jstring     VITAARC_SOFTFP (*NewString)(JNIEnv*, const jchar*, jsize);
    jsize       VITAARC_SOFTFP (*GetStringLength)(JNIEnv*, jstring);
    const jchar* VITAARC_SOFTFP (*GetStringChars)(JNIEnv*, jstring, jboolean*);
    void        VITAARC_SOFTFP (*ReleaseStringChars)(JNIEnv*, jstring, const jchar*);
    jstring     VITAARC_SOFTFP (*NewStringUTF)(JNIEnv*, const char*);
    jsize       VITAARC_SOFTFP (*GetStringUTFLength)(JNIEnv*, jstring);
    /* JNI spec says this returns const jbyte*, but that's inconsistent */
    const char* VITAARC_SOFTFP (*GetStringUTFChars)(JNIEnv*, jstring, jboolean*);
    void        VITAARC_SOFTFP (*ReleaseStringUTFChars)(JNIEnv*, jstring, const char*);
    jsize       VITAARC_SOFTFP (*GetArrayLength)(JNIEnv*, jarray);
    jobjectArray VITAARC_SOFTFP (*NewObjectArray)(JNIEnv*, jsize, jclass, jobject);
    jobject     VITAARC_SOFTFP (*GetObjectArrayElement)(JNIEnv*, jobjectArray, jsize);
    void        VITAARC_SOFTFP (*SetObjectArrayElement)(JNIEnv*, jobjectArray, jsize, jobject);

    jbooleanArray VITAARC_SOFTFP (*NewBooleanArray)(JNIEnv*, jsize);
    jbyteArray    VITAARC_SOFTFP (*NewByteArray)(JNIEnv*, jsize);
    jcharArray    VITAARC_SOFTFP (*NewCharArray)(JNIEnv*, jsize);
    jshortArray   VITAARC_SOFTFP (*NewShortArray)(JNIEnv*, jsize);
    jintArray     VITAARC_SOFTFP (*NewIntArray)(JNIEnv*, jsize);
    jlongArray    VITAARC_SOFTFP (*NewLongArray)(JNIEnv*, jsize);
    jfloatArray   VITAARC_SOFTFP (*NewFloatArray)(JNIEnv*, jsize);
    jdoubleArray  VITAARC_SOFTFP (*NewDoubleArray)(JNIEnv*, jsize);

    jboolean*   VITAARC_SOFTFP (*GetBooleanArrayElements)(JNIEnv*, jbooleanArray, jboolean*);
    jbyte*      VITAARC_SOFTFP (*GetByteArrayElements)(JNIEnv*, jbyteArray, jboolean*);
    jchar*      VITAARC_SOFTFP (*GetCharArrayElements)(JNIEnv*, jcharArray, jboolean*);
    jshort*     VITAARC_SOFTFP (*GetShortArrayElements)(JNIEnv*, jshortArray, jboolean*);
    jint*       VITAARC_SOFTFP (*GetIntArrayElements)(JNIEnv*, jintArray, jboolean*);
    jlong*      VITAARC_SOFTFP (*GetLongArrayElements)(JNIEnv*, jlongArray, jboolean*);
    jfloat*     VITAARC_SOFTFP (*GetFloatArrayElements)(JNIEnv*, jfloatArray, jboolean*);
    jdouble*    VITAARC_SOFTFP (*GetDoubleArrayElements)(JNIEnv*, jdoubleArray, jboolean*);

    void        VITAARC_SOFTFP (*ReleaseBooleanArrayElements)(JNIEnv*, jbooleanArray,
                        jboolean*, jint);
    void        VITAARC_SOFTFP (*ReleaseByteArrayElements)(JNIEnv*, jbyteArray,
                        jbyte*, jint);
    void        VITAARC_SOFTFP (*ReleaseCharArrayElements)(JNIEnv*, jcharArray,
                        jchar*, jint);
    void        VITAARC_SOFTFP (*ReleaseShortArrayElements)(JNIEnv*, jshortArray,
                        jshort*, jint);
    void        VITAARC_SOFTFP (*ReleaseIntArrayElements)(JNIEnv*, jintArray,
                        jint*, jint);
    void        VITAARC_SOFTFP (*ReleaseLongArrayElements)(JNIEnv*, jlongArray,
                        jlong*, jint);
    void        VITAARC_SOFTFP (*ReleaseFloatArrayElements)(JNIEnv*, jfloatArray,
                        jfloat*, jint);
    void        VITAARC_SOFTFP (*ReleaseDoubleArrayElements)(JNIEnv*, jdoubleArray,
                        jdouble*, jint);

    void        VITAARC_SOFTFP (*GetBooleanArrayRegion)(JNIEnv*, jbooleanArray,
                        jsize, jsize, jboolean*);
    void        VITAARC_SOFTFP (*GetByteArrayRegion)(JNIEnv*, jbyteArray,
                        jsize, jsize, jbyte*);
    void        VITAARC_SOFTFP (*GetCharArrayRegion)(JNIEnv*, jcharArray,
                        jsize, jsize, jchar*);
    void        VITAARC_SOFTFP (*GetShortArrayRegion)(JNIEnv*, jshortArray,
                        jsize, jsize, jshort*);
    void        VITAARC_SOFTFP (*GetIntArrayRegion)(JNIEnv*, jintArray,
                        jsize, jsize, jint*);
    void        VITAARC_SOFTFP (*GetLongArrayRegion)(JNIEnv*, jlongArray,
                        jsize, jsize, jlong*);
    void        VITAARC_SOFTFP (*GetFloatArrayRegion)(JNIEnv*, jfloatArray,
                        jsize, jsize, jfloat*);
    void        VITAARC_SOFTFP (*GetDoubleArrayRegion)(JNIEnv*, jdoubleArray,
                        jsize, jsize, jdouble*);

    /* spec shows these without const; some jni.h do, some don't */
    void        VITAARC_SOFTFP (*SetBooleanArrayRegion)(JNIEnv*, jbooleanArray,
                        jsize, jsize, const jboolean*);
    void        VITAARC_SOFTFP (*SetByteArrayRegion)(JNIEnv*, jbyteArray,
                        jsize, jsize, const jbyte*);
    void        VITAARC_SOFTFP (*SetCharArrayRegion)(JNIEnv*, jcharArray,
                        jsize, jsize, const jchar*);
    void        VITAARC_SOFTFP (*SetShortArrayRegion)(JNIEnv*, jshortArray,
                        jsize, jsize, const jshort*);
    void        VITAARC_SOFTFP (*SetIntArrayRegion)(JNIEnv*, jintArray,
                        jsize, jsize, const jint*);
    void        VITAARC_SOFTFP (*SetLongArrayRegion)(JNIEnv*, jlongArray,
                        jsize, jsize, const jlong*);
    void        VITAARC_SOFTFP (*SetFloatArrayRegion)(JNIEnv*, jfloatArray,
                        jsize, jsize, const jfloat*);
    void        VITAARC_SOFTFP (*SetDoubleArrayRegion)(JNIEnv*, jdoubleArray,
                        jsize, jsize, const jdouble*);

    jint        VITAARC_SOFTFP (*RegisterNatives)(JNIEnv*, jclass, const JNINativeMethod*,
                        jint);
    jint        VITAARC_SOFTFP (*UnregisterNatives)(JNIEnv*, jclass);
    jint        VITAARC_SOFTFP (*MonitorEnter)(JNIEnv*, jobject);
    jint        VITAARC_SOFTFP (*MonitorExit)(JNIEnv*, jobject);
    jint        VITAARC_SOFTFP (*GetJavaVM)(JNIEnv*, JavaVM**);

    void        VITAARC_SOFTFP (*GetStringRegion)(JNIEnv*, jstring, jsize, jsize, jchar*);
    void        VITAARC_SOFTFP (*GetStringUTFRegion)(JNIEnv*, jstring, jsize, jsize, char*);

    void*       VITAARC_SOFTFP (*GetPrimitiveArrayCritical)(JNIEnv*, jarray, jboolean*);
    void        VITAARC_SOFTFP (*ReleasePrimitiveArrayCritical)(JNIEnv*, jarray, void*, jint);

    const jchar* VITAARC_SOFTFP (*GetStringCritical)(JNIEnv*, jstring, jboolean*);
    void        VITAARC_SOFTFP (*ReleaseStringCritical)(JNIEnv*, jstring, const jchar*);

    jweak       VITAARC_SOFTFP (*NewWeakGlobalRef)(JNIEnv*, jobject);
    void        VITAARC_SOFTFP (*DeleteWeakGlobalRef)(JNIEnv*, jweak);

    jboolean    VITAARC_SOFTFP (*ExceptionCheck)(JNIEnv*);

    jobject     VITAARC_SOFTFP (*NewDirectByteBuffer)(JNIEnv*, void*, jlong);
    void*       VITAARC_SOFTFP (*GetDirectBufferAddress)(JNIEnv*, jobject);
    jlong       VITAARC_SOFTFP (*GetDirectBufferCapacity)(JNIEnv*, jobject);

    /* added in JNI 1.6 */
    jobjectRefType VITAARC_SOFTFP (*GetObjectRefType)(JNIEnv*, jobject);
};

/*
 * C++ object wrapper.
 *
 * This is usually overlaid on a C struct whose first element is a
 * JNINativeInterface*.  We rely somewhat on compiler behavior.
 */
struct _JNIEnv {
    /* do not rename this; it does not seem to be entirely opaque */
    const struct JNINativeInterface* functions;

#if defined(__cplusplus)

    jint GetVersion()
    { return functions->GetVersion(this); }

    jclass DefineClass(const char *name, jobject loader, const jbyte* buf,
        jsize bufLen)
    { return functions->DefineClass(this, name, loader, buf, bufLen); }

    jclass FindClass(const char* name)
    { return functions->FindClass(this, name); }

    jmethodID FromReflectedMethod(jobject method)
    { return functions->FromReflectedMethod(this, method); }

    jfieldID FromReflectedField(jobject field)
    { return functions->FromReflectedField(this, field); }

    jobject ToReflectedMethod(jclass cls, jmethodID methodID, jboolean isStatic)
    { return functions->ToReflectedMethod(this, cls, methodID, isStatic); }

    jclass GetSuperclass(jclass clazz)
    { return functions->GetSuperclass(this, clazz); }

    jboolean IsAssignableFrom(jclass clazz1, jclass clazz2)
    { return functions->IsAssignableFrom(this, clazz1, clazz2); }

    jobject ToReflectedField(jclass cls, jfieldID fieldID, jboolean isStatic)
    { return functions->ToReflectedField(this, cls, fieldID, isStatic); }

    jint Throw(jthrowable obj)
    { return functions->Throw(this, obj); }

    jint ThrowNew(jclass clazz, const char* message)
    { return functions->ThrowNew(this, clazz, message); }

    jthrowable ExceptionOccurred()
    { return functions->ExceptionOccurred(this); }

    void ExceptionDescribe()
    { functions->ExceptionDescribe(this); }

    void ExceptionClear()
    { functions->ExceptionClear(this); }

    void FatalError(const char* msg)
    { functions->FatalError(this, msg); }

    jint PushLocalFrame(jint capacity)
    { return functions->PushLocalFrame(this, capacity); }

    jobject PopLocalFrame(jobject result)
    { return functions->PopLocalFrame(this, result); }

    jobject NewGlobalRef(jobject obj)
    { return functions->NewGlobalRef(this, obj); }

    void DeleteGlobalRef(jobject globalRef)
    { functions->DeleteGlobalRef(this, globalRef); }

    void DeleteLocalRef(jobject localRef)
    { functions->DeleteLocalRef(this, localRef); }

    jboolean IsSameObject(jobject ref1, jobject ref2)
    { return functions->IsSameObject(this, ref1, ref2); }

    jobject NewLocalRef(jobject ref)
    { return functions->NewLocalRef(this, ref); }

    jint EnsureLocalCapacity(jint capacity)
    { return functions->EnsureLocalCapacity(this, capacity); }

    jobject AllocObject(jclass clazz)
    { return functions->AllocObject(this, clazz); }

    jobject NewObject(jclass clazz, jmethodID methodID, ...)
    {
        va_list args;
        va_start(args, methodID);
        jobject result = functions->NewObjectV(this, clazz, methodID, args);
        va_end(args);
        return result;
    }

    jobject NewObjectV(jclass clazz, jmethodID methodID, va_list args)
    { return functions->NewObjectV(this, clazz, methodID, args); }

    jobject NewObjectA(jclass clazz, jmethodID methodID, const jvalue* args)
    { return functions->NewObjectA(this, clazz, methodID, args); }

    jclass GetObjectClass(jobject obj)
    { return functions->GetObjectClass(this, obj); }

    jboolean IsInstanceOf(jobject obj, jclass clazz)
    { return functions->IsInstanceOf(this, obj, clazz); }

    jmethodID GetMethodID(jclass clazz, const char* name, const char* sig)
    { return functions->GetMethodID(this, clazz, name, sig); }

#define CALL_TYPE_METHOD(_jtype, _jname)                                    \
    _jtype Call##_jname##Method(jobject obj, jmethodID methodID, ...)       \
    {                                                                       \
        _jtype result;                                                      \
        va_list args;                                                       \
        va_start(args, methodID);                                           \
        result = functions->Call##_jname##MethodV(this, obj, methodID,      \
                    args);                                                  \
        va_end(args);                                                       \
        return result;                                                      \
    }
#define CALL_TYPE_METHODV(_jtype, _jname)                                   \
    _jtype Call##_jname##MethodV(jobject obj, jmethodID methodID,           \
        va_list args)                                                       \
    { return functions->Call##_jname##MethodV(this, obj, methodID, args); }
#define CALL_TYPE_METHODA(_jtype, _jname)                                   \
    _jtype Call##_jname##MethodA(jobject obj, jmethodID methodID,           \
                                 const jvalue* args)                        \
    { return functions->Call##_jname##MethodA(this, obj, methodID, args); }

#define CALL_TYPE(_jtype, _jname)                                           \
    CALL_TYPE_METHOD(_jtype, _jname)                                        \
    CALL_TYPE_METHODV(_jtype, _jname)                                       \
    CALL_TYPE_METHODA(_jtype, _jname)

    CALL_TYPE(jobject, Object)
    CALL_TYPE(jboolean, Boolean)
    CALL_TYPE(jbyte, Byte)
    CALL_TYPE(jchar, Char)
    CALL_TYPE(jshort, Short)
    CALL_TYPE(jint, Int)
    CALL_TYPE(jlong, Long)
    CALL_TYPE(jfloat, Float)
    CALL_TYPE(jdouble, Double)

    void CallVoidMethod(jobject obj, jmethodID methodID, ...)
    {
        va_list args;
        va_start(args, methodID);
        functions->CallVoidMethodV(this, obj, methodID, args);
        va_end(args);
    }
    void CallVoidMethodV(jobject obj, jmethodID methodID, va_list args)
    { functions->CallVoidMethodV(this, obj, methodID, args); }
    void CallVoidMethodA(jobject obj, jmethodID methodID, const jvalue* args)
    { functions->CallVoidMethodA(this, obj, methodID, args); }

#define CALL_NONVIRT_TYPE_METHOD(_jtype, _jname)                            \
    _jtype CallNonvirtual##_jname##Method(jobject obj, jclass clazz,        \
        jmethodID methodID, ...)                                            \
    {                                                                       \
        _jtype result;                                                      \
        va_list args;                                                       \
        va_start(args, methodID);                                           \
        result = functions->CallNonvirtual##_jname##MethodV(this, obj,      \
                    clazz, methodID, args);                                 \
        va_end(args);                                                       \
        return result;                                                      \
    }
#define CALL_NONVIRT_TYPE_METHODV(_jtype, _jname)                           \
    _jtype CallNonvirtual##_jname##MethodV(jobject obj, jclass clazz,       \
        jmethodID methodID, va_list args)                                   \
    { return functions->CallNonvirtual##_jname##MethodV(this, obj, clazz,   \
        methodID, args); }
#define CALL_NONVIRT_TYPE_METHODA(_jtype, _jname)                           \
    _jtype CallNonvirtual##_jname##MethodA(jobject obj, jclass clazz,       \
        jmethodID methodID, const jvalue* args)                             \
    { return functions->CallNonvirtual##_jname##MethodA(this, obj, clazz,   \
        methodID, args); }

#define CALL_NONVIRT_TYPE(_jtype, _jname)                                   \
    CALL_NONVIRT_TYPE_METHOD(_jtype, _jname)                                \
    CALL_NONVIRT_TYPE_METHODV(_jtype, _jname)                               \
    CALL_NONVIRT_TYPE_METHODA(_jtype, _jname)

    CALL_NONVIRT_TYPE(jobject, Object)
    CALL_NONVIRT_TYPE(jboolean, Boolean)
    CALL_NONVIRT_TYPE(jbyte, Byte)
    CALL_NONVIRT_TYPE(jchar, Char)
    CALL_NONVIRT_TYPE(jshort, Short)
    CALL_NONVIRT_TYPE(jint, Int)
    CALL_NONVIRT_TYPE(jlong, Long)
    CALL_NONVIRT_TYPE(jfloat, Float)
    CALL_NONVIRT_TYPE(jdouble, Double)

    void CallNonvirtualVoidMethod(jobject obj, jclass clazz,
        jmethodID methodID, ...)
    {
        va_list args;
        va_start(args, methodID);
        functions->CallNonvirtualVoidMethodV(this, obj, clazz, methodID, args);
        va_end(args);
    }
    void CallNonvirtualVoidMethodV(jobject obj, jclass clazz,
        jmethodID methodID, va_list args)
    { functions->CallNonvirtualVoidMethodV(this, obj, clazz, methodID, args); }
    void CallNonvirtualVoidMethodA(jobject obj, jclass clazz,
        jmethodID methodID, const jvalue* args)
    { functions->CallNonvirtualVoidMethodA(this, obj, clazz, methodID, args); }

    jfieldID GetFieldID(jclass clazz, const char* name, const char* sig)
    { return functions->GetFieldID(this, clazz, name, sig); }

    jobject GetObjectField(jobject obj, jfieldID fieldID)
    { return functions->GetObjectField(this, obj, fieldID); }
    jboolean GetBooleanField(jobject obj, jfieldID fieldID)
    { return functions->GetBooleanField(this, obj, fieldID); }
    jbyte GetByteField(jobject obj, jfieldID fieldID)
    { return functions->GetByteField(this, obj, fieldID); }
    jchar GetCharField(jobject obj, jfieldID fieldID)
    { return functions->GetCharField(this, obj, fieldID); }
    jshort GetShortField(jobject obj, jfieldID fieldID)
    { return functions->GetShortField(this, obj, fieldID); }
    jint GetIntField(jobject obj, jfieldID fieldID)
    { return functions->GetIntField(this, obj, fieldID); }
    jlong GetLongField(jobject obj, jfieldID fieldID)
    { return functions->GetLongField(this, obj, fieldID); }
    jfloat GetFloatField(jobject obj, jfieldID fieldID)
    { return functions->GetFloatField(this, obj, fieldID); }
    jdouble GetDoubleField(jobject obj, jfieldID fieldID)
    { return functions->GetDoubleField(this, obj, fieldID); }

    void SetObjectField(jobject obj, jfieldID fieldID, jobject value)
    { functions->SetObjectField(this, obj, fieldID, value); }
    void SetBooleanField(jobject obj, jfieldID fieldID, jboolean value)
    { functions->SetBooleanField(this, obj, fieldID, value); }
    void SetByteField(jobject obj, jfieldID fieldID, jbyte value)
    { functions->SetByteField(this, obj, fieldID, value); }
    void SetCharField(jobject obj, jfieldID fieldID, jchar value)
    { functions->SetCharField(this, obj, fieldID, value); }
    void SetShortField(jobject obj, jfieldID fieldID, jshort value)
    { functions->SetShortField(this, obj, fieldID, value); }
    void SetIntField(jobject obj, jfieldID fieldID, jint value)
    { functions->SetIntField(this, obj, fieldID, value); }
    void SetLongField(jobject obj, jfieldID fieldID, jlong value)
    { functions->SetLongField(this, obj, fieldID, value); }
    void SetFloatField(jobject obj, jfieldID fieldID, jfloat value)
    { functions->SetFloatField(this, obj, fieldID, value); }
    void SetDoubleField(jobject obj, jfieldID fieldID, jdouble value)
    { functions->SetDoubleField(this, obj, fieldID, value); }

    jmethodID GetStaticMethodID(jclass clazz, const char* name, const char* sig)
    { return functions->GetStaticMethodID(this, clazz, name, sig); }

#define CALL_STATIC_TYPE_METHOD(_jtype, _jname)                             \
    _jtype CallStatic##_jname##Method(jclass clazz, jmethodID methodID,     \
        ...)                                                                \
    {                                                                       \
        _jtype result;                                                      \
        va_list args;                                                       \
        va_start(args, methodID);                                           \
        result = functions->CallStatic##_jname##MethodV(this, clazz,        \
                    methodID, args);                                        \
        va_end(args);                                                       \
        return result;                                                      \
    }
#define CALL_STATIC_TYPE_METHODV(_jtype, _jname)                            \
    _jtype CallStatic##_jname##MethodV(jclass clazz, jmethodID methodID,    \
        va_list args)                                                       \
    { return functions->CallStatic##_jname##MethodV(this, clazz, methodID,  \
        args); }
#define CALL_STATIC_TYPE_METHODA(_jtype, _jname)                            \
    _jtype CallStatic##_jname##MethodA(jclass clazz, jmethodID methodID,    \
                                       const jvalue* args)                  \
    { return functions->CallStatic##_jname##MethodA(this, clazz, methodID,  \
        args); }

#define CALL_STATIC_TYPE(_jtype, _jname)                                    \
    CALL_STATIC_TYPE_METHOD(_jtype, _jname)                                 \
    CALL_STATIC_TYPE_METHODV(_jtype, _jname)                                \
    CALL_STATIC_TYPE_METHODA(_jtype, _jname)

    CALL_STATIC_TYPE(jobject, Object)
    CALL_STATIC_TYPE(jboolean, Boolean)
    CALL_STATIC_TYPE(jbyte, Byte)
    CALL_STATIC_TYPE(jchar, Char)
    CALL_STATIC_TYPE(jshort, Short)
    CALL_STATIC_TYPE(jint, Int)
    CALL_STATIC_TYPE(jlong, Long)
    CALL_STATIC_TYPE(jfloat, Float)
    CALL_STATIC_TYPE(jdouble, Double)

    void CallStaticVoidMethod(jclass clazz, jmethodID methodID, ...)
    {
        va_list args;
        va_start(args, methodID);
        functions->CallStaticVoidMethodV(this, clazz, methodID, args);
        va_end(args);
    }
    void CallStaticVoidMethodV(jclass clazz, jmethodID methodID, va_list args)
    { functions->CallStaticVoidMethodV(this, clazz, methodID, args); }
    void CallStaticVoidMethodA(jclass clazz, jmethodID methodID, const jvalue* args)
    { functions->CallStaticVoidMethodA(this, clazz, methodID, args); }

    jfieldID GetStaticFieldID(jclass clazz, const char* name, const char* sig)
    { return functions->GetStaticFieldID(this, clazz, name, sig); }

    jobject GetStaticObjectField(jclass clazz, jfieldID fieldID)
    { return functions->GetStaticObjectField(this, clazz, fieldID); }
    jboolean GetStaticBooleanField(jclass clazz, jfieldID fieldID)
    { return functions->GetStaticBooleanField(this, clazz, fieldID); }
    jbyte GetStaticByteField(jclass clazz, jfieldID fieldID)
    { return functions->GetStaticByteField(this, clazz, fieldID); }
    jchar GetStaticCharField(jclass clazz, jfieldID fieldID)
    { return functions->GetStaticCharField(this, clazz, fieldID); }
    jshort GetStaticShortField(jclass clazz, jfieldID fieldID)
    { return functions->GetStaticShortField(this, clazz, fieldID); }
    jint GetStaticIntField(jclass clazz, jfieldID fieldID)
    { return functions->GetStaticIntField(this, clazz, fieldID); }
    jlong GetStaticLongField(jclass clazz, jfieldID fieldID)
    { return functions->GetStaticLongField(this, clazz, fieldID); }
    jfloat GetStaticFloatField(jclass clazz, jfieldID fieldID)
    { return functions->GetStaticFloatField(this, clazz, fieldID); }
    jdouble GetStaticDoubleField(jclass clazz, jfieldID fieldID)
    { return functions->GetStaticDoubleField(this, clazz, fieldID); }

    void SetStaticObjectField(jclass clazz, jfieldID fieldID, jobject value)
    { functions->SetStaticObjectField(this, clazz, fieldID, value); }
    void SetStaticBooleanField(jclass clazz, jfieldID fieldID, jboolean value)
    { functions->SetStaticBooleanField(this, clazz, fieldID, value); }
    void SetStaticByteField(jclass clazz, jfieldID fieldID, jbyte value)
    { functions->SetStaticByteField(this, clazz, fieldID, value); }
    void SetStaticCharField(jclass clazz, jfieldID fieldID, jchar value)
    { functions->SetStaticCharField(this, clazz, fieldID, value); }
    void SetStaticShortField(jclass clazz, jfieldID fieldID, jshort value)
    { functions->SetStaticShortField(this, clazz, fieldID, value); }
    void SetStaticIntField(jclass clazz, jfieldID fieldID, jint value)
    { functions->SetStaticIntField(this, clazz, fieldID, value); }
    void SetStaticLongField(jclass clazz, jfieldID fieldID, jlong value)
    { functions->SetStaticLongField(this, clazz, fieldID, value); }
    void SetStaticFloatField(jclass clazz, jfieldID fieldID, jfloat value)
    { functions->SetStaticFloatField(this, clazz, fieldID, value); }
    void SetStaticDoubleField(jclass clazz, jfieldID fieldID, jdouble value)
    { functions->SetStaticDoubleField(this, clazz, fieldID, value); }

    jstring NewString(const jchar* unicodeChars, jsize len)
    { return functions->NewString(this, unicodeChars, len); }

    jsize GetStringLength(jstring string)
    { return functions->GetStringLength(this, string); }

    const jchar* GetStringChars(jstring string, jboolean* isCopy)
    { return functions->GetStringChars(this, string, isCopy); }

    void ReleaseStringChars(jstring string, const jchar* chars)
    { functions->ReleaseStringChars(this, string, chars); }

    jstring NewStringUTF(const char* bytes)
    { return functions->NewStringUTF(this, bytes); }

    jsize GetStringUTFLength(jstring string)
    { return functions->GetStringUTFLength(this, string); }

    const char* GetStringUTFChars(jstring string, jboolean* isCopy)
    { return functions->GetStringUTFChars(this, string, isCopy); }

    void ReleaseStringUTFChars(jstring string, const char* utf)
    { functions->ReleaseStringUTFChars(this, string, utf); }

    jsize GetArrayLength(jarray array)
    { return functions->GetArrayLength(this, array); }

    jobjectArray NewObjectArray(jsize length, jclass elementClass,
        jobject initialElement)
    { return functions->NewObjectArray(this, length, elementClass,
        initialElement); }

    jobject GetObjectArrayElement(jobjectArray array, jsize index)
    { return functions->GetObjectArrayElement(this, array, index); }

    void SetObjectArrayElement(jobjectArray array, jsize index, jobject value)
    { functions->SetObjectArrayElement(this, array, index, value); }

    jbooleanArray NewBooleanArray(jsize length)
    { return functions->NewBooleanArray(this, length); }
    jbyteArray NewByteArray(jsize length)
    { return functions->NewByteArray(this, length); }
    jcharArray NewCharArray(jsize length)
    { return functions->NewCharArray(this, length); }
    jshortArray NewShortArray(jsize length)
    { return functions->NewShortArray(this, length); }
    jintArray NewIntArray(jsize length)
    { return functions->NewIntArray(this, length); }
    jlongArray NewLongArray(jsize length)
    { return functions->NewLongArray(this, length); }
    jfloatArray NewFloatArray(jsize length)
    { return functions->NewFloatArray(this, length); }
    jdoubleArray NewDoubleArray(jsize length)
    { return functions->NewDoubleArray(this, length); }

    jboolean* GetBooleanArrayElements(jbooleanArray array, jboolean* isCopy)
    { return functions->GetBooleanArrayElements(this, array, isCopy); }
    jbyte* GetByteArrayElements(jbyteArray array, jboolean* isCopy)
    { return functions->GetByteArrayElements(this, array, isCopy); }
    jchar* GetCharArrayElements(jcharArray array, jboolean* isCopy)
    { return functions->GetCharArrayElements(this, array, isCopy); }
    jshort* GetShortArrayElements(jshortArray array, jboolean* isCopy)
    { return functions->GetShortArrayElements(this, array, isCopy); }
    jint* GetIntArrayElements(jintArray array, jboolean* isCopy)
    { return functions->GetIntArrayElements(this, array, isCopy); }
    jlong* GetLongArrayElements(jlongArray array, jboolean* isCopy)
    { return functions->GetLongArrayElements(this, array, isCopy); }
    jfloat* GetFloatArrayElements(jfloatArray array, jboolean* isCopy)
    { return functions->GetFloatArrayElements(this, array, isCopy); }
    jdouble* GetDoubleArrayElements(jdoubleArray array, jboolean* isCopy)
    { return functions->GetDoubleArrayElements(this, array, isCopy); }

    void ReleaseBooleanArrayElements(jbooleanArray array, jboolean* elems,
        jint mode)
    { functions->ReleaseBooleanArrayElements(this, array, elems, mode); }
    void ReleaseByteArrayElements(jbyteArray array, jbyte* elems,
        jint mode)
    { functions->ReleaseByteArrayElements(this, array, elems, mode); }
    void ReleaseCharArrayElements(jcharArray array, jchar* elems,
        jint mode)
    { functions->ReleaseCharArrayElements(this, array, elems, mode); }
    void ReleaseShortArrayElements(jshortArray array, jshort* elems,
        jint mode)
    { functions->ReleaseShortArrayElements(this, array, elems, mode); }
    void ReleaseIntArrayElements(jintArray array, jint* elems,
        jint mode)
    { functions->ReleaseIntArrayElements(this, array, elems, mode); }
    void ReleaseLongArrayElements(jlongArray array, jlong* elems,
        jint mode)
    { functions->ReleaseLongArrayElements(this, array, elems, mode); }
    void ReleaseFloatArrayElements(jfloatArray array, jfloat* elems,
        jint mode)
    { functions->ReleaseFloatArrayElements(this, array, elems, mode); }
    void ReleaseDoubleArrayElements(jdoubleArray array, jdouble* elems,
        jint mode)
    { functions->ReleaseDoubleArrayElements(this, array, elems, mode); }

    void GetBooleanArrayRegion(jbooleanArray array, jsize start, jsize len,
        jboolean* buf)
    { functions->GetBooleanArrayRegion(this, array, start, len, buf); }
    void GetByteArrayRegion(jbyteArray array, jsize start, jsize len,
        jbyte* buf)
    { functions->GetByteArrayRegion(this, array, start, len, buf); }
    void GetCharArrayRegion(jcharArray array, jsize start, jsize len,
        jchar* buf)
    { functions->GetCharArrayRegion(this, array, start, len, buf); }
    void GetShortArrayRegion(jshortArray array, jsize start, jsize len,
        jshort* buf)
    { functions->GetShortArrayRegion(this, array, start, len, buf); }
    void GetIntArrayRegion(jintArray array, jsize start, jsize len,
        jint* buf)
    { functions->GetIntArrayRegion(this, array, start, len, buf); }
    void GetLongArrayRegion(jlongArray array, jsize start, jsize len,
        jlong* buf)
    { functions->GetLongArrayRegion(this, array, start, len, buf); }
    void GetFloatArrayRegion(jfloatArray array, jsize start, jsize len,
        jfloat* buf)
    { functions->GetFloatArrayRegion(this, array, start, len, buf); }
    void GetDoubleArrayRegion(jdoubleArray array, jsize start, jsize len,
        jdouble* buf)
    { functions->GetDoubleArrayRegion(this, array, start, len, buf); }

    void SetBooleanArrayRegion(jbooleanArray array, jsize start, jsize len,
        const jboolean* buf)
    { functions->SetBooleanArrayRegion(this, array, start, len, buf); }
    void SetByteArrayRegion(jbyteArray array, jsize start, jsize len,
        const jbyte* buf)
    { functions->SetByteArrayRegion(this, array, start, len, buf); }
    void SetCharArrayRegion(jcharArray array, jsize start, jsize len,
        const jchar* buf)
    { functions->SetCharArrayRegion(this, array, start, len, buf); }
    void SetShortArrayRegion(jshortArray array, jsize start, jsize len,
        const jshort* buf)
    { functions->SetShortArrayRegion(this, array, start, len, buf); }
    void SetIntArrayRegion(jintArray array, jsize start, jsize len,
        const jint* buf)
    { functions->SetIntArrayRegion(this, array, start, len, buf); }
    void SetLongArrayRegion(jlongArray array, jsize start, jsize len,
        const jlong* buf)
    { functions->SetLongArrayRegion(this, array, start, len, buf); }
    void SetFloatArrayRegion(jfloatArray array, jsize start, jsize len,
        const jfloat* buf)
    { functions->SetFloatArrayRegion(this, array, start, len, buf); }
    void SetDoubleArrayRegion(jdoubleArray array, jsize start, jsize len,
        const jdouble* buf)
    { functions->SetDoubleArrayRegion(this, array, start, len, buf); }

    jint RegisterNatives(jclass clazz, const JNINativeMethod* methods,
        jint nMethods)
    { return functions->RegisterNatives(this, clazz, methods, nMethods); }

    jint UnregisterNatives(jclass clazz)
    { return functions->UnregisterNatives(this, clazz); }

    jint MonitorEnter(jobject obj)
    { return functions->MonitorEnter(this, obj); }

    jint MonitorExit(jobject obj)
    { return functions->MonitorExit(this, obj); }

    jint GetJavaVM(JavaVM** vm)
    { return functions->GetJavaVM(this, vm); }

    void GetStringRegion(jstring str, jsize start, jsize len, jchar* buf)
    { functions->GetStringRegion(this, str, start, len, buf); }

    void GetStringUTFRegion(jstring str, jsize start, jsize len, char* buf)
    { return functions->GetStringUTFRegion(this, str, start, len, buf); }

    void* GetPrimitiveArrayCritical(jarray array, jboolean* isCopy)
    { return functions->GetPrimitiveArrayCritical(this, array, isCopy); }

    void ReleasePrimitiveArrayCritical(jarray array, void* carray, jint mode)
    { functions->ReleasePrimitiveArrayCritical(this, array, carray, mode); }

    const jchar* GetStringCritical(jstring string, jboolean* isCopy)
    { return functions->GetStringCritical(this, string, isCopy); }

    void ReleaseStringCritical(jstring string, const jchar* carray)
    { functions->ReleaseStringCritical(this, string, carray); }

    jweak NewWeakGlobalRef(jobject obj)
    { return functions->NewWeakGlobalRef(this, obj); }

    void DeleteWeakGlobalRef(jweak obj)
    { functions->DeleteWeakGlobalRef(this, obj); }

    jboolean ExceptionCheck()
    { return functions->ExceptionCheck(this); }

    jobject NewDirectByteBuffer(void* address, jlong capacity)
    { return functions->NewDirectByteBuffer(this, address, capacity); }

    void* GetDirectBufferAddress(jobject buf)
    { return functions->GetDirectBufferAddress(this, buf); }

    jlong GetDirectBufferCapacity(jobject buf)
    { return functions->GetDirectBufferCapacity(this, buf); }

    /* added in JNI 1.6 */
    jobjectRefType GetObjectRefType(jobject obj)
    { return functions->GetObjectRefType(this, obj); }
#endif /*__cplusplus*/
};


/*
 * JNI invocation interface.
 */
struct JNIInvokeInterface {
    void*       reserved0;
    void*       reserved1;
    void*       reserved2;

    jint        VITAARC_SOFTFP (*DestroyJavaVM)(JavaVM*);
    jint        VITAARC_SOFTFP (*AttachCurrentThread)(JavaVM*, JNIEnv**, void*);
    jint        VITAARC_SOFTFP (*DetachCurrentThread)(JavaVM*);
    jint        VITAARC_SOFTFP (*GetEnv)(JavaVM*, void**, jint);
    jint        VITAARC_SOFTFP (*AttachCurrentThreadAsDaemon)(JavaVM*, JNIEnv**, void*);
};

/*
 * C++ version.
 */
struct _JavaVM {
    const struct JNIInvokeInterface* functions;

#if defined(__cplusplus)
    jint DestroyJavaVM()
    { return functions->DestroyJavaVM(this); }
    jint AttachCurrentThread(JNIEnv** p_env, void* thr_args)
    { return functions->AttachCurrentThread(this, p_env, thr_args); }
    jint DetachCurrentThread()
    { return functions->DetachCurrentThread(this); }
    jint GetEnv(void** env, jint version)
    { return functions->GetEnv(this, env, version); }
    jint AttachCurrentThreadAsDaemon(JNIEnv** p_env, void* thr_args)
    { return functions->AttachCurrentThreadAsDaemon(this, p_env, thr_args); }
#endif /*__cplusplus*/
};

struct JavaVMAttachArgs {
    jint        version;    /* must be >= JNI_VERSION_1_2 */
    const char* name;       /* NULL or name of thread as modified UTF-8 str */
    jobject     group;      /* global ref of a ThreadGroup object, or NULL */
};
typedef struct JavaVMAttachArgs JavaVMAttachArgs;

/*
 * JNI 1.2+ initialization.  (As of 1.6, the pre-1.2 structures are no
 * longer supported.)
 */
typedef struct JavaVMOption {
    const char* optionString;
    void*       extraInfo;
} JavaVMOption;

typedef struct JavaVMInitArgs {
    jint        version;    /* use JNI_VERSION_1_2 or later */

    jint        nOptions;
    JavaVMOption* options;
    jboolean    ignoreUnrecognized;
} JavaVMInitArgs;

#ifdef __cplusplus
extern "C" {
#endif
/*
 * VM initialization functions.
 *
 * Note these are the only symbols exported for JNI by the VM.
 */
jint JNI_GetDefaultJavaVMInitArgs(void*);
jint JNI_CreateJavaVM(JavaVM**, JNIEnv**, void*);
jint JNI_GetCreatedJavaVMs(JavaVM**, jsize, jsize*);

#define JNIIMPORT typedef
#define JNIEXPORT  __attribute__ ((visibility ("default")))
#define JNICALL

/*
 * Prototypes for functions exported by loadable shared libs.  These are
 * called by JNI, not provided by JNI.
 */
JNIIMPORT jint (*JNI_OnLoad)(JavaVM* vm, void* reserved);
JNIIMPORT void (*JNI_OnUnload)(JavaVM* vm, void* reserved);

#ifdef __cplusplus
}
#endif


/*
 * Manifest constants.
 */
#define JNI_FALSE   0
#define JNI_TRUE    1

#define JNI_VERSION_1_1 0x00010001
#define JNI_VERSION_1_2 0x00010002
#define JNI_VERSION_1_4 0x00010004
#define JNI_VERSION_1_6 0x00010006

#define JNI_OK          (0)         /* no error */
#define JNI_ERR         (-1)        /* generic error */
#define JNI_EDETACHED   (-2)        /* thread detached from the VM */
#define JNI_EVERSION    (-3)        /* JNI version error */
#define JNI_ENOMEM      (-4)        /* Out of memory */
#define JNI_EEXIST      (-5)        /* VM already created */
#define JNI_EINVAL      (-6)        /* Invalid argument */

#define JNI_COMMIT      1           /* copy content, do not free buffer */
#define JNI_ABORT       2           /* free buffer w/o copying back */

#endif  /* __BRIDGES_JNI_AOSP_JNI_H */
