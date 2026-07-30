#include <logcat/logcat.h>
#include <config.h>

#include "cocos.h"
#include "runtime.h"

void load_class_method(void)
{
  log_v(TAG, "Called loadClassMethod()");
}

jstring get_string_for_key(char *key, char *default_value)
{
  (void)key;
  log_v(TAG, "Called getStringForKey()");
  return (jstring)default_value;
}

jstring get_cocos2dx_package_name(void)
{
  log_v(TAG, "Called getCocos2dxPackageName()");
  return jni_runtime_new_string_utf("moe.low.arc");
}

jstring get_current_language(void)
{
  log_v(TAG, "Called getCurrentLanguage()");
  return jni_runtime_new_string_utf("en");
}

jstring get_cocos2dx_writable_path(void)
{
  log_v(TAG, "Called getCocos2dxWritablePath()");
  return jni_runtime_new_string_utf(PATH_TO_DATA);
}

void set_string_for_key(jstring key, jstring value)
{
  (void)key;
  (void)value;
  log_v(TAG, "Called setStringForKey()");
}

jstring generate_guid(void)
{
  log_v(TAG, "Called generateGuid()");
  return jni_runtime_new_string_utf("61616161");
}
