#ifndef __BRIDGES_JNI_COCOS_H
#define __BRIDGES_JNI_COCOS_H

#include <wchar.h>
#include "aosp/jni.h"

void load_class_method(void);

jstring get_string_for_key(char *key, char *default_value);

jstring get_cocos2dx_package_name(void);

jstring get_cocos2dx_writable_path(void);

jstring get_current_language(void);

void set_string_for_key(jstring key, jstring value);

jstring generate_guid(void);

#endif /* __BRIDGES_JNI_COCOS_H */
