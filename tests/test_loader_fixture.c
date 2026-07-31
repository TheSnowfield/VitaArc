#include <stdint.h>
#include <string.h>

#ifdef LOADER_PROVIDER
unsigned provider_data = 31;
unsigned provider_trace;
unsigned provider_destroyed, provider_bad_order;

unsigned provider_add(unsigned value)
{
    return provider_data + value;
}

void provider_event(unsigned event)
{
    provider_trace = provider_trace * 10 + event;
}

void provider_destructor(void* argument)
{
    if((unsigned)(uintptr_t)argument != 127 - provider_destroyed) {
        provider_bad_order = 1;
    }

    ++provider_destroyed;
}

/* SHN_ABS symbols must not acquire the library's load bias. */
/* The fixture linker defines fixture_absolute with --defsym. */
extern char fixture_absolute[];
uintptr_t provider_absolute_address(void)
{
    return (uintptr_t)fixture_absolute;
}

#else
extern unsigned provider_data;
extern unsigned provider_add(unsigned);
extern void provider_event(unsigned);
extern void provider_destructor(void*);
extern int __cxa_atexit(void (*)(void*), void*, void*);
extern char fixture_absolute[];
extern unsigned optional_data __attribute__((weak));
extern unsigned optional_function(void) __attribute__((weak));

unsigned fixture_data = 17;
unsigned fixture_bss[64];
unsigned* fixture_relative = &fixture_data;
unsigned* fixture_external = &provider_data;

__attribute__((constructor(101))) static void __first_constructor(void)
{
    provider_event(1);
}

__attribute__((constructor(102))) static void __second_constructor(void)
{
    provider_event(2);
}

__attribute__((constructor(103))) static void __register_destructors(void)
{
    for(unsigned index = 0; index < 128; ++index) {
        if(__cxa_atexit(provider_destructor, (void*)(uintptr_t)index, &fixture_data)) {
            provider_event(9);
        }
    }
}

__attribute__((destructor(101))) static void __first_destructor(void)
{
    provider_event(3);
}

__attribute__((destructor(102))) static void __second_destructor(void)
{
    provider_event(4);
}

uintptr_t fixture_absolute_address(void)
{
    return (uintptr_t)fixture_absolute;
}

unsigned fixture_run(unsigned value)
{
    for(unsigned index = 0; index < sizeof(fixture_bss) / sizeof(fixture_bss[0]); ++index) {
        if(fixture_bss[index]) {
            return 0;
        }
    }

    if(&optional_data || optional_function) {
        return 0;
    }

#ifdef LOADER_MISSING_IMPORT
    (void)value;
    extern unsigned required_missing_function(void);
    return required_missing_function();

#else
    return provider_add(value) + *fixture_relative + *fixture_external + strlen("bridge");

#endif
}

#endif
