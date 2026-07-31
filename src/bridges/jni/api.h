/* Expand api.def once for the entry points and once for the function table.
 * All implementation calls share thread validation and the runtime lock. */
#define JNI_VALUE(type, entry, name, implementation, parameters, arguments)                                            \
    static VITAARC_SOFTFP type jni_entry_api_##name parameters                                                         \
    {                                                                                                                  \
        if(!jni_thread_enter(env)) {                                                                                   \
            return (type)0;                                                                                            \
        }                                                                                                              \
        type result = implementation arguments;                                                                        \
        jni_thread_leave();                                                                                            \
        return result;                                                                                                 \
    }

#define JNI_STATUS(type, entry, name, implementation, parameters, arguments)                                           \
    static VITAARC_SOFTFP type jni_entry_api_##name parameters                                                         \
    {                                                                                                                  \
        if(!jni_thread_enter(env)) {                                                                                   \
            return JNI_ERR;                                                                                            \
        }                                                                                                              \
        type result = implementation arguments;                                                                        \
        jni_thread_leave();                                                                                            \
        return result;                                                                                                 \
    }

#define JNI_REF(type, entry, name, implementation, parameters, arguments)                                              \
    static VITAARC_SOFTFP type jni_entry_api_##name parameters                                                         \
    {                                                                                                                  \
        if(!jni_thread_enter(env)) {                                                                                   \
            return NULL;                                                                                               \
        }                                                                                                              \
        type result = (type)jni_local(env, implementation arguments);                                                  \
        jni_thread_leave();                                                                                            \
        return result;                                                                                                 \
    }

#define JNI_VOID(entry, name, implementation, parameters, arguments)                                                   \
    static VITAARC_SOFTFP void jni_entry_api_##name parameters                                                         \
    {                                                                                                                  \
        if(!jni_thread_enter(env)) {                                                                                   \
            return;                                                                                                    \
        }                                                                                                              \
        implementation arguments;                                                                                      \
        jni_thread_leave();                                                                                            \
    }

#define JNI_VAR_VALUE(type, entry, name, implementation, parameters, arguments, last)                                  \
    static VITAARC_SOFTFP type jni_entry_api_##name parameters                                                         \
    {                                                                                                                  \
        if(!jni_thread_enter(env)) {                                                                                   \
            return (type)0;                                                                                            \
        }                                                                                                              \
        va_list values;                                                                                                \
        va_start(values, last);                                                                                        \
        type result = implementation arguments;                                                                        \
        va_end(values);                                                                                                \
        jni_thread_leave();                                                                                            \
        return result;                                                                                                 \
    }

#define JNI_VAR_REF(type, entry, name, implementation, parameters, arguments, last)                                    \
    static VITAARC_SOFTFP type jni_entry_api_##name parameters                                                         \
    {                                                                                                                  \
        if(!jni_thread_enter(env)) {                                                                                   \
            return NULL;                                                                                               \
        }                                                                                                              \
        va_list values;                                                                                                \
        va_start(values, last);                                                                                        \
        type result = (type)jni_local(env, implementation arguments);                                                  \
        va_end(values);                                                                                                \
        jni_thread_leave();                                                                                            \
        return result;                                                                                                 \
    }

#define JNI_VAR_VOID(entry, name, implementation, parameters, arguments, last)                                         \
    static VITAARC_SOFTFP void jni_entry_api_##name parameters                                                         \
    {                                                                                                                  \
        if(!jni_thread_enter(env)) {                                                                                   \
            return;                                                                                                    \
        }                                                                                                              \
        va_list values;                                                                                                \
        va_start(values, last);                                                                                        \
        implementation arguments;                                                                                      \
        va_end(values);                                                                                                \
        jni_thread_leave();                                                                                            \
    }

#include "api.def"
#undef JNI_VALUE
#undef JNI_STATUS
#undef JNI_REF
#undef JNI_VOID
#undef JNI_VAR_VALUE
#undef JNI_VAR_REF
#undef JNI_VAR_VOID
