#ifndef VITAARC_JNI_RUNTIME_ENTRY_H
#define VITAARC_JNI_RUNTIME_ENTRY_H

#include <stdarg.h>
#include <abi/calling.h>
#include "../runtime.h"

/* Internal implementations called only from serialized JNI entry wrappers.
 * Variadic wrappers delegate to the V implementation declared by api.def. */
#define JNI_VALUE(type, entry, name, implementation, parameters, arguments)                                            \
    VITAARC_SOFTFP type implementation parameters;
#define JNI_STATUS JNI_VALUE
#define JNI_REF JNI_VALUE
#define JNI_VOID(entry, name, implementation, parameters, arguments) void implementation parameters;
#define JNI_VAR_VALUE(type, entry, name, implementation, parameters, arguments, last)
#define JNI_VAR_REF JNI_VAR_VALUE
#define JNI_VAR_VOID(entry, name, implementation, parameters, arguments, last)
#include "../api.def"
#undef JNI_VALUE
#undef JNI_STATUS
#undef JNI_REF
#undef JNI_VOID
#undef JNI_VAR_VALUE
#undef JNI_VAR_REF
#undef JNI_VAR_VOID

#endif
