#include <stddef.h>
#include <reent.h>
#include <string.h>
#include <jni/aosp/jni.h>

/* Model CRT's native per-thread newlib state using this SDK's actual layout. */
size_t test_reent_size(void)
{
    return sizeof(struct _reent);
}

void test_initialize_reent(struct _reent* state)
{
    _REENT_INIT_PTR(state);
}

int test_pending_exception(JNIEnv* env)
{
    return (*env)->ExceptionCheck(env);
}

int test_destroy_vm(JavaVM* vm)
{
    return (*vm)->DestroyJavaVM(vm);
}
