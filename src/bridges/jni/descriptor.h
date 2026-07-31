#ifndef __BRIDGES_JNI_DESCRIPTOR_H
#define __BRIDGES_JNI_DESCRIPTOR_H

#include <stdbool.h>
#include <stddef.h>

#define JNI_MAX_CALL_ARGS 255

/**
 * @brief Parsed JNI types.
 * @note References and arrays use L; long/double occupy two slots.
 */
typedef struct jni_sig {
    char args[JNI_MAX_CALL_ARGS];
    size_t argc;
    size_t slots;
    char ret;
} jni_sig_t;

/**
 * @brief Parse a complete method signature without consuming call arguments.
 */
bool jni_sig_parse(const char* text, jni_sig_t* sig);

/**
 * @brief Parse one field type, rejecting void and trailing characters.
 */
bool jni_type_parse(const char* text, char* type);

#endif /* __BRIDGES_JNI_DESCRIPTOR_H */
