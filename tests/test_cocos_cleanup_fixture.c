#include <stdint.h>
#include <stdlib.h>

extern void provider_event(unsigned event);
uintptr_t application_vtable[4] __asm__("_ZTV11AppDelegate");
void* application __asm__("_ZN7cocos2d11Application21sm_pSharedApplicationE");
static void* factory;
static void* foreign_provider;
extern uintptr_t audio_vtable[] __asm__("_ZTV24AudioProviderFMODAndroid");

/* Legacy AudioManager forwarding ABI: provider at +0x1e4, update at slot 15.
 * The loader checks this caller before selecting the imported provider table. */
__asm__(".pushsection .text\n"
        ".syntax unified\n"
        ".thumb\n"
        ".align 2\n"
        ".global _ZN12AudioManager6updateEf\n"
        ".type _ZN12AudioManager6updateEf, %function\n"
        ".thumb_func\n"
        "_ZN12AudioManager6updateEf:\n"
        "ldr.w r0, [r0, #0x1e4]\n"
        "ldr r3, [r0]\n"
        "ldr r3, [r3, #60]\n"
        "bx r3\n"
        ".size _ZN12AudioManager6updateEf, .-_ZN12AudioManager6updateEf\n"
        ".popsection\n");

typedef struct application_prefix {
    uintptr_t* vtable;
    void* provider;
} application_prefix_t;

void set_foreign_provider(void)
{
    foreign_provider = ((application_prefix_t*)application)->provider;
    *(uintptr_t**)foreign_provider = application_vtable;
}

void delete_application(void* object) __asm__("_ZN11AppDelegateD0Ev");
void delete_application(void* object)
{
    if(object != application) {
        provider_event(9);
        return;
    }

    application = NULL;
    free(object);
    provider_event(3);
}

void destroy_factory(void) __asm__("_ZN7cocos2d13ObjectFactory15destroyInstanceEv");
void destroy_factory(void)
{
    if(factory) {
        free(factory);
        factory = NULL;
        provider_event(4);
    }
}

__attribute__((constructor)) static void __initialize(void)
{
    application_prefix_t* object = malloc(sizeof(*object));
    object->vtable = application_vtable + 2;
    object->provider = calloc(1, 2 * sizeof(void*));
    *(uintptr_t**)object->provider = audio_vtable + 2;
    application = object;
    factory = malloc(64);
    provider_event(1);
}

__attribute__((destructor)) static void __finalize(void)
{
    if(application) {
        free(((application_prefix_t*)application)->provider);
        foreign_provider = NULL;
        free(application);
        application = NULL;
        provider_event(2);
    }

    if(foreign_provider) {
        free(foreign_provider);
        foreign_provider = NULL;
        provider_event(6);
    }

    destroy_factory();
    provider_event(5);
}
