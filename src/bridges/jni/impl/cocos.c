#include "../../../common/define.h"
#include "../../../logcat/logcat.h"
#include "cocos.h"

char *getCocos2dxPackageName()
{
  logV(TAG, "Called getCocos2dxPackageName()");
  return "moe.low.arc";
}

char *getCurrentLanguage()
{
  logV(TAG, "Called getCurrentLanguage()");
  return "en";
}

void loadClassMethod()
{
  logV(TAG, "Called loadClassMethod()");
}

char *getStringForKey(char *szKey, char *szDefaultValue)
{
  logV(TAG, "Called getStringForKey()");
  return "NULL";
}
