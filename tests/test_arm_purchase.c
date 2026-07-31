#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <dynalib.h>

typedef struct purchase_api {
    void* (*allocate)(unsigned size);
    void (*deallocate)(void* pointer);
    void (*copy_string)(void* destination, const void* source);
    void (*destroy_string)(void* string);
    void* owner;
    uintptr_t* base_type;
} purchase_api_t;

typedef struct function_storage {
    uintptr_t data[2];
    bool (*manager)(struct function_storage*, const struct function_storage*, int);
    void (*invoke)(const void*, const void*, const void*);
} function_storage_t;

_Static_assert(sizeof(function_storage_t) == 16, "ARM game std::function storage");

typedef struct capture {
    uint32_t string;
    uint32_t magic;
} capture_t;

_Static_assert(sizeof(capture_t) == 8, "fixture-owned COW capture");

static purchase_api_t __api;
uint32_t purchase_fixture_stats[5]; /* clone, destroy, invoke, wrapper destroy, violations */

static bool __manage(function_storage_t* destination, const function_storage_t* source, int operation)
{
    capture_t* capture = (void*)source->data[0];
    if(operation == 2) {
        capture_t* copy = __api.allocate(sizeof(*copy));
        __api.copy_string(&copy->string, &capture->string);
        copy->magic = 0xcafed00e;
        destination->data[0] = (uintptr_t)copy;
        destination->data[1] = 0x11223344;
        ++purchase_fixture_stats[0];
    } else if(operation == 3) {
        if(capture->magic != 0xcafed00d && capture->magic != 0xcafed00e) {
            ++purchase_fixture_stats[4];
        }

        if(capture->magic == 0xcafed00e && *(uintptr_t**)__api.owner != __api.base_type) {
            ++purchase_fixture_stats[4];
        }

        __api.destroy_string(&capture->string);
        capture->magic = 0;
        __api.deallocate(capture);
        ++purchase_fixture_stats[1];
    } else if(operation == 1) {
        destination->data[0] = source->data[0];
    } else {
        destination->data[0] = 0;
    }

    return false;
}

static void __invoke(const void* data, const void* success, const void* result)
{
    (void)data;
    (void)success;
    (void)result;
    ++purchase_fixture_stats[2];
}

/* A fixture-owned function target is cloned by the actual game's std::function
 * copy constructor and destroyed by its actual _Function_base destructor. */
void test_purchase_function_source(function_storage_t* function, const void* string, const purchase_api_t* api)
{
    __api = *api;
    capture_t* capture = __api.allocate(sizeof(*capture));
    __api.copy_string(&capture->string, string);
    capture->magic = 0xcafed00d;
    *function = (function_storage_t){.data = {(uintptr_t)capture, 0x11223344}, .manager = __manage, .invoke = __invoke};
}

typedef struct wrapper_owner {
    void* wrapper;
    void** slot;
    void (*destroy)(void*);
    void* expected_listener;
} wrapper_owner_t;

void test_destroy_purchase_wrapper(void* argument)
{
    wrapper_owner_t* owner = argument;
    if(*owner->slot != owner->wrapper || ((void**)owner->wrapper)[3] != owner->expected_listener) {
        ++purchase_fixture_stats[4];
    }

    *owner->slot = NULL;
    ++purchase_fixture_stats[3];
    owner->destroy(owner->wrapper);
}

uint32_t test_purchase_symbol_size(dynalib_t* library, const char* name, uint32_t size)
{
    for(uint32_t index = 0; index < library->nsyms; ++index) {
        Elf32_Sym* symbol = &library->symbols[index];
        if(!strcmp((const char*)library->strings + symbol->st_name, name)) {
            uint32_t previous = symbol->st_size;
            symbol->st_size = size;
            return previous;
        }
    }

    return 0;
}
