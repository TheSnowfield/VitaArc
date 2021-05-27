#include "../../../logcat/logcat.h"
#include "../../../common/define.h"
#include "../../../utils/debug.h"
#include "jvm.h"

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

jint AttachCurrentThreadAsDaemon(JavaVM *javaVM, JNIEnv **jniEnv, void *a1)
{
  logV(TAG, "Called AttachCurrentThreadAsDaemon");
  return 0;
}
