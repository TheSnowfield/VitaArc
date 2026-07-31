#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <config.h>
#include <libc/posix.h>

#include "object.h"
#include "entry.h"
#include "class.h"
#include "exception.h"
#include "method.h"
#include "services.h"
#include "../cocos.h"
#include "../preferences.h"

static jobject __new_class_loader(JNIEnv* env)
{
    return (jobject)jni_obj_new(env, jni_entry_find_class(env, JCLS_LOADER));
}

static bool __invoke_string(JNIEnv* env, jni_obj_t* object, jni_obj_t* method, const jvalue* args, jvalue* result)
{
    if(object->kind != JNI_KIND_STRING) {
        return false;
    }

    if(!jni_owner_is(env, method, JCLS_STRING)) {
        return false;
    }

    if(jni_method_is(method, "length", "()I")) {
        result->i = object->length;
    } else if(jni_method_is(method, "isEmpty", "()Z")) {
        result->z = object->length == 0;
    } else if(jni_method_is(method, "charAt", "(I)C")) {
        jint index = args[0].i;
        if(index < 0 || index >= object->length) {
            jni_raise(env, JEX_STRING_BOUNDS, "charAt index");
        } else {
            result->c = object->utf16[index];
        }
    } else if(jni_method_is(method, "equals", "(Ljava/lang/Object;)Z")) {
        jni_obj_t* other = jni_obj_get(args[0].l);
        result->z = JNI_FALSE;
        if(other && other->kind == JNI_KIND_STRING && other->length == object->length) {
            result->z = memcmp(object->utf16, other->utf16, (size_t)object->length * sizeof(jchar)) == 0;
        }
    } else if(jni_method_is(method, "toString", "()Ljava/lang/String;")) {
        result->l = (jobject)object;
    } else {
        return false;
    }

    return true;
}

static bool __invoke_context(JNIEnv* env, jni_obj_t* object, jni_obj_t* method, const jvalue* args, jvalue* result)
{
    (void)object;
    (void)args;
    if(!jni_owner_is(env, method, "android/content/Context")) {
        return false;
    }

    if(jni_method_is(method, "getClassLoader", "()Ljava/lang/ClassLoader;")) {
        result->l = __new_class_loader(env);
    } else if(jni_method_is(method, "getAssets", "()Landroid/content/res/AssetManager;")) {
        result->l = jni_cocos_get_asset_manager();
    } else if(jni_method_is(method, "getApplicationContext", "()Landroid/content/Context;")) {
        result->l = jni_cocos_get_context();
    } else if(jni_method_is(method, "getPackageName", "()Ljava/lang/String;")) {
        result->l = jni_entry_new_string_utf(env, "moe.low.arc");
    } else if(jni_method_is(method, "getFilesDir", "()Ljava/io/File;")) {
        if(!cocos_prepare_files_directory()) {
            jni_raise(env, JEX_IO, "cannot create application data directory");
            return true;
        }

        jni_obj_t* file = jni_obj_new(env, jni_entry_find_class(env, JCLS_FILE));
        if(file) {
            file->utf8 = jni_text_dup(ANDROID_FILES_PATH);
            if(file->utf8) {
                result->l = (jobject)file;
            }
        }
    } else {
        return false;
    }

    return true;
}

static bool __invoke_file(JNIEnv* env, jni_obj_t* object, jni_obj_t* method, const jvalue* args, jvalue* result)
{
    (void)args;
    if(!jni_owner_is(env, method, JCLS_FILE)) {
        return false;
    }

    if(!object->utf8) {
        jni_raise(env, JEX_STATE, "File is not initialized");
        return true;
    }

    if(jni_method_is(method, "getPath", "()Ljava/lang/String;") ||
        jni_method_is(method, "toString", "()Ljava/lang/String;")) {
        result->l = jni_entry_new_string_utf(env, object->utf8);
    } else if(jni_method_is(method, "getAbsolutePath", "()Ljava/lang/String;")) {
        /* Vita device paths are already absolute. Relative File paths require a
         * Java working-directory model which this runtime does not provide. */
        if(!strchr(object->utf8, ':') && object->utf8[0] != '/') {
            return false;
        }

        result->l = jni_entry_new_string_utf(env, object->utf8);
    } else if(jni_method_is(method, "exists", "()Z")) {
        struct stat status;
        char buffer[1024];
        const char* path = bridge_resolve_path(object->utf8, buffer, sizeof(buffer));
        result->z = path && stat(path, &status) == 0;
    } else if(jni_method_is(method, "isDirectory", "()Z")) {
        struct stat status;
        char buffer[1024];
        const char* path = bridge_resolve_path(object->utf8, buffer, sizeof(buffer));
        result->z = JNI_FALSE;
        if(path && stat(path, &status) == 0) {
            result->z = S_ISDIR(status.st_mode);
        }
    } else {
        return false;
    }

    return true;
}

static bool __invoke_class(JNIEnv* env, jni_obj_t* object, jni_obj_t* method, const jvalue* args, jvalue* result)
{
    (void)args;
    if(object->kind != JNI_KIND_CLASS || !jni_owner_is(env, method, JCLS_CLASS)) {
        return false;
    }

    if(jni_method_is(method, "getClassLoader", "()Ljava/lang/ClassLoader;")) {
        result->l = __new_class_loader(env);
    } else if(jni_method_is(method, "getName", "()Ljava/lang/String;")) {
        char* name = jni_text_dup(object->name);
        if(name) {
            for(char* cursor = name; *cursor; ++cursor) {
                if(*cursor == '/') {
                    *cursor = '.';
                }
            }

            result->l = jni_entry_new_string_utf(env, name);
            free(name);
        }
    } else {
        return false;
    }

    return true;
}

static bool __invoke_class_loader(JNIEnv* env, jni_obj_t* object, jni_obj_t* method, const jvalue* args, jvalue* result)
{
    (void)object;
    if(!jni_owner_is(env, method, JCLS_LOADER)) {
        return false;
    }

    if(!jni_method_is(method, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;")) {
        return false;
    }

    const char* name = jni_runtime_get_string_utf(args[0].l);
    if(!name) {
        jni_raise(env, JEX_NULL, "class name is null");
        return true;
    }

    result->l = jni_entry_find_class(env, name);
    return true;
}

static bool __invoke_object(JNIEnv* env, jni_obj_t* object, jni_obj_t* method, const jvalue* args, jvalue* result)
{
    if(!jni_owner_is(env, method, JCLS_OBJECT)) {
        return false;
    }

    if(jni_method_is(method, "getClass", "()Ljava/lang/Class;")) {
        result->l = (jobject)jni_classof(env, object);
    } else if(jni_method_is(method, "equals", "(Ljava/lang/Object;)Z")) {
        result->z = object == jni_obj_get(args[0].l);
    } else {
        return false;
    }

    return true;
}

static bool __invoke_constructor(JNIEnv* env, jni_obj_t* object, jni_obj_t* method, const jvalue* args)
{
    const char* class_name = method->clazz->name;
    if(strcmp(class_name, JCLS_OBJECT) == 0) {
        return jni_method_is(method, "<init>", "()V");
    }

    if(strcmp(class_name, JCLS_FILE) != 0) {
        return false;
    }

    if(!jni_method_is(method, "<init>", "(Ljava/lang/String;)V")) {
        return false;
    }

    const char* path = jni_runtime_get_string_utf(args[0].l);
    if(!path) {
        jni_raise(env, JEX_NULL, "file path is null");
        return true;
    }

    char* copy = jni_text_dup(path);
    if(!copy) {
        jni_raise(env, JEX_MEMORY, "file path allocation failed");
        return true;
    }

    free(object->utf8);
    object->utf8 = copy;
    return true;
}

typedef bool (*jni_service_t)(JNIEnv*, jni_obj_t*, jni_obj_t*, const jvalue*, jvalue*);

static const jni_service_t __services[] = {
    __invoke_string, __invoke_context, __invoke_file, __invoke_class, __invoke_class_loader, __invoke_object};

bool jni_service_call(JNIEnv* env, jni_obj_t* object, jni_obj_t* method, const jvalue* args, jvalue* result)
{
    if(strcmp(method->name, "<init>") == 0) {
        return __invoke_constructor(env, object, method, args);
    }

    for(size_t index = 0; index < sizeof(__services) / sizeof(__services[0]); ++index) {
        if(__services[index](env, object, method, args, result)) {
            return true;
        }
    }

    return false;
}
