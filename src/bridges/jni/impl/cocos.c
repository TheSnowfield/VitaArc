#include "../../../common/define.h"
#include "../../../logcat/logcat.h"

#include "jni.h"
#include "cocos.h"

void loadClassMethod()
{
  logV(TAG, "Called loadClassMethod()");
}

jstring getStringForKey(char *szKey, char *szDefaultValue)
{
  logV(TAG, "Called getStringForKey()");
  return JSTRING("NULL");
}

jstring getCocos2dxPackageName()
{
  logV(TAG, "Called getCocos2dxPackageName()");
  return JSTRING("moe.low.arc");
}

jstring getCurrentLanguage()
{
  logV(TAG, "Called getCurrentLanguage()");
  return JSTRING("en");
}

jstring getCocos2dxWritablePath()
{
  logV(TAG, "Called getCocos2dxWritablePath()");
  return JSTRING("NULL");
}

jstring generateGuid()
{
  logV(TAG, "Called generateGuid()");
  return JSTRING("61616161");
}
