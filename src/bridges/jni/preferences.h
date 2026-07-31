#ifndef __BRIDGES_JNI_PREFERENCES_H
#define __BRIDGES_JNI_PREFERENCES_H

#include <stdbool.h>

/* Returned values belong to the caller. Missing keys return NULL with errno=0.
 * Values are MUTF-8 strings; type is Z/I/F/S (Cocos stores doubles as floats).
 * Each successful mutation is persisted before returning. */
char* cocos_preferences_get(const char* key, char* type);
bool cocos_preferences_set(const char* key, char type, const char* value);
bool cocos_preferences_delete(const char* key);
bool cocos_prepare_data_directory(void);
bool cocos_prepare_files_directory(void);

#endif /* __BRIDGES_JNI_PREFERENCES_H */
