#include <string.h>

#include "../../../logcat/logcat.h"
#include "../../../common/define.h"
#include "../../../utils/debug.h"
#include "cocos.h"
#include "jni.h"

jint GetVersion(JNIEnv *jniEnv)
{
  logV(TAG, "Called GetVersion");
  return 0;
}

jclass FindClass(JNIEnv *jniEnv, const char *a1)
{
  logV(TAG, "Called FindClass");
  return 0;
}

void ExceptionClear(JNIEnv *jniEnv)
{
  logV(TAG, "Called ExceptionClear");
}

jobject NewGlobalRef(JNIEnv *jniEnv, jobject a1)
{
  logV(TAG, "Called NewGlobalRef");
  return (jobject)0xDEADC0DE;
}

void DeleteLocalRef(JNIEnv *jniEnv, jobject a1)
{
  logV(TAG, "Called DeleteLocalRef(0x%08X)", a1);
}

jmethodID GetMethodID(JNIEnv *jniEnv, jclass a1, const char *a2, const char *a3)
{
  logV(TAG, "Called GetMethodID");
}

jmethodID GetStaticMethodID(JNIEnv *jniEnv, jclass a1, const char *a2, const char *a3)
{
  logV(TAG, "Called GetStaticMethodID(0x%08X, 0x%08X, \"%s\", \"%s\")", jniEnv, a1, a2, a3);

  // Search method
  for (int i = 0; i < sizeof(JNI_METHODS) / sizeof(JNIMETHOD); ++i)
  {
    if (strcmp(JNI_METHODS[i].szMethodName, a2) == 0)
      return (jmethodID)JNI_METHODS[i].nMethodId;
  }

  logW(TAG, "Requested method not found.");
  return NULL;
}

jobject CallObjectMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3)
{
  logV(TAG, "Called CallObjectMethodV(0x%08X, 0x%08X, %d, va_list)", jniEnv, a1, a2);

  // cocos2dx use 0 to
  // get load class method
  if (a1 == 0x00000000 && a2 == 0)
    return (jobject)LOAD_CLASS_METHOD;

  return NULL;
}

jobject CallStaticObjectMethodV(JNIEnv *jniEnv, jclass a1, jmethodID a2, va_list a3)
{
  logV(TAG, "Called CallStaticObjectMethodV(0x%08X, 0x%08X, %d, va_list)", jniEnv, a1, a2);

  switch ((METHODID)((uint32_t)a2))
  {
  case GET_COCOS2DX_PACKAGE_NAME:
    return (jobject)getCocos2dxPackageName();
  case GET_CURRENT_LANGUAGE:
    return (jobject)getCurrentLanguage();
  }

  return NULL;
}

jboolean CallBooleanMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3)
{
  logV(TAG, "Called CallBooleanMethodV(0x%08X, 0x%08X, %d, va_list)", jniEnv, a1, a2);
  return NULL;
}

jint CallIntMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3)
{
  logV(TAG, "Called CallIntMethodV");
}

jfloat CallFloatMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3)
{
  logV(TAG, "Called CallFloatMethodV");
}

void CallVoidMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3)
{
  logV(TAG, "Called CallVoidMethodV");
}

jfieldID FromReflectedField(JNIEnv *jniEnv, jobject a1)
{
  logV(TAG, "Called FromReflectedField");
}

jmethodID FromReflectedMethod(JNIEnv *jniEnv, jobject a1)
{
  logV(TAG, "Called FromReflectedMethod");
}

jstring NewStringUTF(JNIEnv *jniEnv, const char *a1)
{
  logV(TAG, "Called NewStringUTF(0x%08X, \"%s\")", jniEnv, a1);
  return (jstring)a1;
}

const jchar *GetStringChars(JNIEnv *jniEnv, jstring a1, jboolean *a2)
{
  logV(TAG, "Called GetStringChars(0x%08X, \"%s\", 0x%08X)", a1, a2);
  return (jchar *)a1;
}

jsize GetStringLength(JNIEnv *jniEnv, jstring a1)
{
  logV(TAG, "Called GetStringLength(\"%s\")", a1);
  return strlen((const char *)a1);
}

const char *GetStringUTFChars(JNIEnv *jniEnv, jstring a1, jboolean *j)
{
  logV(TAG, "Called GetStringUTFChars");
  return (const char *)a1;
}

void ReleaseStringUTFChars(JNIEnv *jniEnv, jstring a1, const char *j)
{
  logV(TAG, "Called ReleaseStringUTFChars");
}

void ReleaseStringChars(JNIEnv *jniEnv, jstring a1, const jchar *a2)
{
  logV(TAG, "Called ReleaseStringChars(0x%08X, \"%s\", \"%s\")", jniEnv, a1, a2);
}

jint RegisterNatives(JNIEnv *jniEnv, jclass a1, const JNINativeMethod *a2, jint a3)
{
  logV(TAG, "Called RegisterNatives");
}

jstring NewString(JNIEnv *jniEnv, const jchar *a1, jsize a2)
{
  logV(TAG, "Called NewString(0x%08X, \"%s\", %d)", jniEnv, a1, a2);

  for (int i = 0; i < a2 * 2; ++i)
  {
    logPrintf("%02X", *((char *)a1 + i));
  }

  return (jstring)a1;
}
