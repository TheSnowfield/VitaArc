#include <solibrary/solib.h>
#include <utils/patcher.h>

#include <logcat/logcat.h>
#include <common/define.h>
#include <utils/debug.h>

#include "jni.h"
#include "user/jni.h"

static struct JNIInvokeInterface jniInvokeEnv;
static struct JNINativeInterface jniNativeEnv;

typedef int (*JNI_OnInit)(void *JNIEnv, bool r1, bool graphics);
typedef int (*_testmain)();
static JNI_OnInit native;

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
  return 0;
}

jobject NewGlobalRef(JNIEnv *jniEnv, jobject a1)
{
  logV(TAG, "Called NewGlobalRef");
  return (void *)0xDEADC0DE;
}

void DeleteLocalRef(JNIEnv *jniEnv, jobject a1)
{
  logV(TAG, "Called DeleteLocalRef");
  return 0;
}

jmethodID GetMethodID(JNIEnv *jniEnv, jclass a1, const char *a2, const char *a3)
{
  logV(TAG, "Called GetMethodID");
}

jobject CallObjectMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3)
{
  logV(TAG, "Called CallObjectMethodV");
}

jboolean CallBooleanMethodV(JNIEnv *jniEnv, jobject a1, jmethodID a2, va_list a3)
{
  logV(TAG, "Called CallBooleanMethodV");
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

jstring NewStringUTF(JNIEnv *jniEnv, const char *a1)
{
  logV(TAG, "Called NewStringUTF");
  return a1;
}

const char *GetStringUTFChars(JNIEnv *jniEnv, jstring a1, jboolean *j)
{
  logV(TAG, "Called GetStringUTFChars");
  return a1;
}

void ReleaseStringUTFChars(JNIEnv *jniEnv, jstring a1, const char *j)
{
  logV(TAG, "Called ReleaseStringUTFChars");
  return 0;
}

jint RegisterNatives(JNIEnv *jniEnv, jclass a1, const JNINativeMethod *a2, jint a3)
{
  logV(TAG, "Called RegisterNatives");
  native = *(void **)(a2 + 8);
}

jint DestroyJavaVM(JavaVM *javaVM)
{
  logV(TAG, "Called DestroyJavaVM");
  return 0;
}

jint AttachCurrentThread(JavaVM *javaVM, JNIEnv **jniEnv, void *a1)
{
  logV(TAG, "Called AttachCurrentThread");
  return 0;
}

jint DetachCurrentThread(JavaVM *javaVM)
{
  logV(TAG, "Called DetachCurrentThread");
  return 0;
}

jint GetEnv(JavaVM *javaVM, void **a1, jint i)
{
  logV(TAG, "Called GetEnv");

  *a1 = &jniNativeEnv;
  return 0;
}

jint AttachCurrentThreadAsDaemon(JavaVM *javaVM, JNIEnv **jniEnv, void *a1)
{
  logV(TAG, "Called AttachCurrentThreadAsDaemon");
  return 0;
}

static struct JNIInvokeInterface jniInvokeEnv =
{
  .reserved0 = &jniInvokeEnv,
  .GetEnv = GetEnv,
  .DestroyJavaVM = DestroyJavaVM,
  .AttachCurrentThread = AttachCurrentThread,
  .DetachCurrentThread = DetachCurrentThread,
  .AttachCurrentThreadAsDaemon = AttachCurrentThreadAsDaemon
};

static struct JNINativeInterface jniNativeEnv =
{
  .reserved0 = &jniNativeEnv,
  .GetVersion = GetVersion,
  .FindClass = FindClass,
  .ExceptionClear = ExceptionClear,
  .NewGlobalRef = NewGlobalRef,
  .DeleteLocalRef = DeleteLocalRef,
  .GetMethodID = GetMethodID,
  .CallObjectMethodV = CallObjectMethodV,
  .CallBooleanMethodV = CallBooleanMethodV,
  .CallIntMethodV = CallIntMethodV,
  .CallFloatMethodV = CallFloatMethodV,
  .CallVoidMethodV = CallVoidMethodV,
  .NewStringUTF = NewStringUTF,
  .GetStringUTFChars = GetStringUTFChars,
  .ReleaseStringUTFChars = ReleaseStringUTFChars,
  .RegisterNatives = RegisterNatives
};

void bridgeCallJNIMain(HSOLIB hSoLibrary)
{
  JNI_OnLoad pfnJNIOnload = solibGetProcAddress(hSoLibrary, "JNI_OnLoad");
  debugMemoryDump(0x98000000, 0x7C8000);

  pfnJNIOnload(&jniInvokeEnv, NULL);
}
