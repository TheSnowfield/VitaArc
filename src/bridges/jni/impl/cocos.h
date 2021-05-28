#ifndef _BRIDGE_JNI_COCOSIMPL_H_
#define _BRIDGE_JNI_COCOSIMPL_H_

#include <wchar.h>
#include "android/jni.h"

void loadClassMethod();

jstring getStringForKey(char *szKey, char *szDefaultValue);

jstring getCocos2dxPackageName();

jstring getCocos2dxWritablePath();

jstring getCurrentLanguage();

#endif /* _BRIDGE_JNI_COCOSIMPL_H_ */
