#ifndef _BRIDGE_JNI_JNIIMPL_JVM_H_
#define _BRIDGE_JNI_JNIIMPL_JVM_H_

#include "android/jni.h"

typedef enum METHODID
{
  UNKNOWN = 0,
  GET_COCOS2DX_PACKAGE_NAME,
  GET_COCOS2DX_WRITABLE_PATH,
  GET_CURRENT_LANGUAGE,
  GET_INTEGER_FOR_KEY,
  GET_STRING_FOR_KEY,
  GET_BOOL_FOR_KEY,
  LOAD_CLASS_METHOD = 0x23333333,
} METHODID;

typedef struct JNIMETHOD
{
  const char *szMethodName;
  const METHODID nMethodId;
} JNIMETHOD;

static const JNIMETHOD JNI_METHODS[] =
{
  {"getCocos2dxPackageName", GET_COCOS2DX_PACKAGE_NAME},
  {"getCocos2dxWritablePath", GET_COCOS2DX_WRITABLE_PATH},
  {"getCurrentLanguage", GET_CURRENT_LANGUAGE},
  {"getIntegerForKey", GET_INTEGER_FOR_KEY},
  {"getStringForKey", GET_STRING_FOR_KEY},
  {"getBoolForKey", GET_BOOL_FOR_KEY},
};

jint GetVersion(JNIEnv *jniEnv);

jclass FindClass(JNIEnv *jniEnv, const char *a1);

void ExceptionClear(JNIEnv *jniEnv);

jobject NewGlobalRef(JNIEnv *jniEnv, jobject a1);

void DeleteLocalRef(JNIEnv *jniEnv, jobject a1);

jmethodID GetMethodID(JNIEnv *jniEnv, jclass a1, const char *a2, const char *a3);

jmethodID GetStaticMethodID(JNIEnv *jniEnv, jclass a1, const char *a2, const char *a3);

jobject CallObjectMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3);

jobject CallStaticObjectMethodV(JNIEnv *jniEnv, jclass a1, jmethodID a2, va_list a3);

jboolean CallBooleanMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3);

jint CallIntMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3);

jfloat CallFloatMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3);

void CallVoidMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3);

jfieldID FromReflectedField(JNIEnv *jniEnv, jobject a1);

jmethodID FromReflectedMethod(JNIEnv *jniEnv, jobject a1);

jstring NewStringUTF(JNIEnv *jniEnv, const char *a1);

const jchar *GetStringChars(JNIEnv *jniEnv, jstring a1, jboolean *a2);

jsize GetStringLength(JNIEnv *jniEnv, jstring a1);

const char *GetStringUTFChars(JNIEnv *jniEnv, jstring a1, jboolean *j);

void ReleaseStringUTFChars(JNIEnv *jniEnv, jstring a1, const char *j);

void ReleaseStringChars(JNIEnv *jniEnv, jstring a1, const jchar *a2);

jint RegisterNatives(JNIEnv *jniEnv, jclass a1, const JNINativeMethod *a2, jint a3);

jstring NewString(JNIEnv*, const jchar*, jsize);

#endif /* _BRIDGE_JNI_JNIIMPL_JVM_H_ */
