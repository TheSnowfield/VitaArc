#ifndef _BRIDGE_JNI_JVMIMPL_H_
#define _BRIDGE_JNI_JVMIMPL_H_

#include "android/jni.h"

jint DestroyJavaVM(JavaVM *javaVM);

jint AttachCurrentThread(JavaVM *javaVM, JNIEnv **jniEnv, void *a1);

jint DetachCurrentThread(JavaVM *javaVM);

jint AttachCurrentThreadAsDaemon(JavaVM *javaVM, JNIEnv **jniEnv, void *a1);

#endif /* _BRIDGE_JNI_JVMIMPL_H_ */