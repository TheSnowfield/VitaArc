#include <stdint.h>
#include "../src/dynalib.h"

uint32_t sync_mutex;
uint32_t sync_condition;
uint32_t sync_finalized;
uint32_t sync_prepare_fails;
uint32_t sync_prepare_calls;
uint32_t sync_after_calls;

static bool __prepare(dynalib_t* library)
{
    (void)library;
    ++sync_prepare_calls;
    return !sync_prepare_fails;
}

void sync_after_finalize(dynalib_t* library)
{
    (void)library;
    sync_after_calls += sync_finalized == 1 ? 1 : 100;
}

void sync_set_hooks(dynalib_t* library)
{
    library->prepare = __prepare;
    library->finish = sync_after_finalize;
}

__attribute__((destructor)) void sync_finish(void)
{
    ++sync_finalized;
}
