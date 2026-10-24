#include <patcher.h>
#include <utils/debug.h>
#include <stdlib.h>
#include <string.h>
#include <loader.h>
#include <logcat/logcat.h>
#include <audio/provider.h>
#include "cocos2dx.h"

typedef struct service_cleanup {
    void (*deallocate)(void*);
    void (*destroy_string)(void*);
    void (*destroy_vector)(void*);
    void (*destroy_function)(void*);
    void (*remove_listener)(void*);
    void* analytics;
    void* achievement;
    void* platform;
    void* purchase;
    uintptr_t* purchase_base_type;
    void* listener_wrapper;
} service_cleanup_t;

static bool __code_bytes_match(dynalib_t* library, uintptr_t address, uint32_t size, uint64_t fingerprint)
{
    if(address < library->base || address - library->base >= library->span ||
        size > library->span - (address - library->base)) {
        return false;
    }

    const unsigned char* code = (const void*)address;
    uint64_t hash = UINT64_C(14695981039346656037);
    for(uint32_t byte = 0; byte < size; ++byte) {
        hash = (hash ^ code[byte]) * UINT64_C(1099511628211);
    }

    return hash == fingerprint;
}

static bool __code_matches(dynalib_t* library, const char* name, uint32_t size, uint64_t fingerprint)
{
    for(uint32_t index = 0; index < library->nsyms; ++index) {
        Elf32_Sym* symbol = &library->symbols[index];
        const char* candidate = (const char*)library->strings + symbol->st_name;
        if(symbol->st_shndx == SHN_UNDEF || strcmp(candidate, name)) {
            continue;
        }

        if(ELF32_ST_TYPE(symbol->st_info) != STT_FUNC || symbol->st_size != size) {
            return false;
        }

        uintptr_t address = (uintptr_t)loader_get_proc_address(library, name) & ~(uintptr_t)1;
        return __code_bytes_match(library, address, size, fingerprint);
    }

    return false;
}

static void (*__string_destructor(dynalib_t* library))(void*)
{
    const char* name = "Java_org_cocos2dx_cpp_AppActivity_setDeviceId";
    if(!__code_matches(library, name, 132, UINT64_C(0xb2c12cf9e38b0837))) {
        return NULL;
    }

    /* The verified caller's +0x42 BL destroys a game std::string. Decode that
     * actual Thumb target rather than assuming the native SDK's COW ABI/empty rep. */
    uintptr_t call = ((uintptr_t)loader_get_proc_address(library, name) & ~(uintptr_t)1) + 0x42;
    const uint16_t* instruction = (const void*)call;
    uint16_t first = instruction[0], second = instruction[1];
    if((first & 0xf800) != 0xf000 || (second & 0xd000) != 0xd000) {
        return NULL;
    }

    uint32_t sign = (first >> 10) & 1;
    uint32_t immediate = (((second >> 13) ^ sign ^ 1) & 1) << 23 | (((second >> 11) ^ sign ^ 1) & 1) << 22 |
        (uint32_t)(first & 0x3ff) << 12 | (uint32_t)(second & 0x7ff) << 1;
    int32_t offset = (int32_t)immediate - (sign ? INT32_C(0x1000000) : 0);
    int64_t target = (int64_t)call + 4 + offset;
    if(target < 0 || target > UINTPTR_MAX ||
        !__code_bytes_match(library, (uintptr_t)target, 88, UINT64_C(0xe008284efb12123d))) {
        return NULL;
    }

    return (void (*)(void*))((uintptr_t)target | 1);
}

/* Negative means callback disconnection cannot be established: keep the live
 * module intact. Zero can still prepare a supported listener disconnection. */
static int __prepare_purchase(dynalib_t* library, void* purchase, service_cleanup_t* state)
{
    uintptr_t* type = loader_get_proc_address(library, "_ZTV21PurchaseManagerMobile");
    uintptr_t* base_type = loader_get_proc_address(library, "_ZTV15PurchaseManager");
    if(!purchase || !type || !base_type || *(uintptr_t**)purchase != type + 2 ||
        *(uintptr_t**)((unsigned char*)purchase + 92) != type + 15) {
        return false;
    }

    if(!__code_matches(library, "_ZN6sdkbox10IAPWrapper11getInstanceEv", 176, UINT64_C(0x3c6d5d23e700dd91))) {
        return -1;
    }

    /* Inspect getInstance's verified cached-pointer load, without invoking the
     * factory (which could initialize unrelated SDK/Java services at shutdown). */
    uintptr_t getter =
        (uintptr_t)loader_get_proc_address(library, "_ZN6sdkbox10IAPWrapper11getInstanceEv") & ~(uintptr_t)1;
    int64_t slot = (int64_t)getter + 0x10 + *(const int32_t*)(getter + 0x9c);
    int64_t returned_slot = (int64_t)getter + 0x88 + *(const int32_t*)(getter + 0xac);
    if(slot != returned_slot || slot < library->base || slot - library->base > library->span - sizeof(void*)) {
        return -1;
    }

    void* wrapper = *(void**)(uintptr_t)slot;
    if(wrapper) {
        uintptr_t* enabled = loader_get_proc_address(library, "_ZTVN6sdkbox17IAPWrapperEnabledE");
        uintptr_t* disabled = loader_get_proc_address(library, "_ZTVN6sdkbox18IAPWrapperDisabledE");
        if(enabled && *(uintptr_t**)wrapper == enabled + 2 &&
            __code_matches(library, "_ZNK6sdkbox17IAPWrapperEnabled11getListenerEv", 4, UINT64_C(0x439f5dc1f6099ee8)) &&
            __code_matches(
                library, "_ZN6sdkbox17IAPWrapperEnabled14removeListenerEv", 6, UINT64_C(0x37c58024a2a4611e))) {
            void* (*get_listener)(const void*) =
                loader_get_proc_address(library, "_ZNK6sdkbox17IAPWrapperEnabled11getListenerEv");
            if(get_listener(wrapper) == (unsigned char*)purchase + 92) {
                state->listener_wrapper = wrapper;
                state->remove_listener =
                    loader_get_proc_address(library, "_ZN6sdkbox17IAPWrapperEnabled14removeListenerEv");
            }
        } else if(!disabled || *(uintptr_t**)wrapper != disabled + 2 ||

            !__code_matches(
                library, "_ZNK6sdkbox18IAPWrapperDisabled11getListenerEv", 4, UINT64_C(0x621eaf803aa017a0))) {
            return -1;
        }
    }

    /* These callers establish the exact string/vector/function member layout.
     * Both member destructors are also verified independently before use. */
    if(!state->destroy_string ||
        !__code_matches(
            library, "_ZN15PurchaseManager28loadPurchasesFromSecureStoreEv", 136, UINT64_C(0xc0e33152e5d18761)) ||
        !__code_matches(library,
            "_ZN15PurchaseManager27attemptNotifyMemoryPurchaseESt8functionIFvb14PurchaseResultEE",
            252,
            UINT64_C(0x9afecf386f73907d)) ||
        !__code_matches(library,
            "_ZN21PurchaseManagerMobile23onProductRequestSuccessERKSt6vectorIN6sdkbox7ProductESaIS2_EE",
            228,
            UINT64_C(0xabdc66c0cbf8ac44)) ||
        !__code_matches(library,
            "_ZN21PurchaseManagerMobile16purchaseMemoriesESt8functionIFvb14PurchaseResultEEb",
            116,
            UINT64_C(0xd1b55eac2db07e29)) ||
        !__code_matches(library, "_ZNSt6vectorISsSaISsEED1Ev", 34, UINT64_C(0x98024b2cb6bf4f69)) ||
        !__code_matches(library, "_ZNSt14_Function_baseD1Ev", 18, UINT64_C(0x181b68899ac4cd89))) {
        return 0;
    }

    state->destroy_vector = loader_get_proc_address(library, "_ZNSt6vectorISsSaISsEED1Ev");
    state->destroy_function = loader_get_proc_address(library, "_ZNSt14_Function_baseD1Ev");
    state->purchase = purchase;
    state->purchase_base_type = base_type + 2;
    return true;
}

static void __cleanup_cocos_services(dynalib_t* library)
{
    service_cleanup_t* state = library->context;
    library->context = NULL;
    if(state) {
        if(state->purchase) {
            unsigned char* purchase = state->purchase;
            /* Captured-target destruction observes base-class dispatch during base
             * member teardown, as it would in the corresponding C++ destructor. */
            *(uintptr_t**)purchase = state->purchase_base_type;
            const unsigned tail_strings[] = {88, 84, 80};

            const unsigned vectors[] = {68, 56, 44};

            const unsigned head_strings[] = {20, 8, 4};

            for(unsigned index = 0; index < 3; ++index) {
                state->destroy_string(purchase + tail_strings[index]);
            }

            for(unsigned index = 0; index < 3; ++index) {
                state->destroy_vector(purchase + vectors[index]);
            }

            state->destroy_function(purchase + 24);
            for(unsigned index = 0; index < 3; ++index) {
                state->destroy_string(purchase + head_strings[index]);
            }

            state->deallocate(purchase);
        }

        if(state->analytics) {
            state->deallocate(state->analytics);
        }

        if(state->achievement) {
            state->deallocate(state->achievement);
        }

        if(state->platform) {
            /* The verified 20-byte platform object owns strings at +4/+8 followed
             * by a data-check bool and billing integer. Destroy members in reverse
             * order using the game's own empty-rep/refcount/deallocation semantics. */
            state->destroy_string((unsigned char*)state->platform + 8);
            state->destroy_string((unsigned char*)state->platform + 4);
            state->deallocate(state->platform);
        }

        free(state);
    }
}

static bool __prepare_cocos_services(dynalib_t* library, void** application)
{
    /* Local 1.6.1c bootstrap allocates these two complete four-byte objects and
     * the matching AppDelegate stores them at +12/+20. Neither concrete type
     * contains resource members or an emitted destructor. Other versions and
     * concrete types need their own ownership proof, not guessed vtable calls. */
    if(!__code_matches(library, "_Z22cocos_android_app_initP7_JNIEnv", 360, UINT64_C(0x1ee9d44584d9d779)) ||
        !__code_matches(library,
            "_ZN11AppDelegateC1EP13AudioProviderP15PurchaseManagerP16AnalyticsManagerP13PlatformUtilsP18AchievementManager",
            48,
            UINT64_C(0x1d6e2cdc22bccc4a))) {
        return true;
    }

    uintptr_t* analytics_type = loader_get_proc_address(library, "_ZTV23AnalyticsManagerAndroid");
    uintptr_t* achievement_type = loader_get_proc_address(library, "_ZTV24AchievementManagerMobile");
    uintptr_t* platform_type = loader_get_proc_address(library, "_ZTV20PlatformUtilsAndroid");
    void (*deallocate)(void*) = loader_get_proc_address(library, "_ZdlPv");
    void* analytics = application[3];
    void* achievement = application[5];
    void* platform = application[4];
    bool owns_analytics = analytics && analytics_type && *(uintptr_t**)analytics == analytics_type + 2;
    bool owns_achievement = achievement && achievement_type && *(uintptr_t**)achievement == achievement_type + 2;
    void (*destroy_string)(void*) = __string_destructor(library);
    bool owns_platform = destroy_string && platform && platform_type && *(uintptr_t**)platform == platform_type + 2;
    service_cleanup_t pending = {.deallocate = deallocate,
        .destroy_string = destroy_string,
        .analytics = owns_analytics ? analytics : NULL,
        .achievement = owns_achievement ? achievement : NULL,
        .platform = owns_platform ? platform : NULL};

    if(__prepare_purchase(library, application[2], &pending) < 0) {
        return false;
    }

    if(!deallocate ||
        (!pending.analytics && !pending.achievement && !pending.platform && !pending.purchase &&
            !pending.listener_wrapper)) {
        return true;
    }

    service_cleanup_t* state = calloc(1, sizeof(*state));
    if(!state) {
        return false;
    }

    *state = pending;
    library->context = state;
    library->finish = __cleanup_cocos_services;
    return true;
}

static bool __cleanup_cocos(dynalib_t* library)
{
    /* The Android bootstrap allocates AppDelegate, but its process-lifetime
     * allocation is not covered by the DSO's C++ static finalizers. Use the
     * exported deleting destructor only for the matching concrete vtable. */
    void** application = loader_get_proc_address(library, "_ZN7cocos2d11Application21sm_pSharedApplicationE");
    uintptr_t* vtable = loader_get_proc_address(library, "_ZTV11AppDelegate");
    void (*delete_application)(void*) = loader_get_proc_address(library, "_ZN11AppDelegateD0Ev");
    if(application && *application && vtable && delete_application) {
        if(*(uintptr_t**)*application == vtable + 2) {
            /* Prepare before any destructive operation. OOM preserves ownership and
             * the live module, rather than losing service pointers during deletion. */
            if(!__prepare_cocos_services(library, *application)) {
                return false;
            }

            service_cleanup_t* state = library->context;
            if(state && state->listener_wrapper) {
                state->remove_listener(state->listener_wrapper);
                state->listener_wrapper = NULL;
            }

            /* The supported AppDelegate constructor stores its injected AudioProvider
             * immediately after the Application vptr. The deleting destructor does
             * not own that service. Release only our exact provider implementation. */
            void* provider = ((void**)*application)[1];
            bool owns_provider = provider && *(uintptr_t**)provider == &vita_audio_provider_vtable[2];
            if(provider && !owns_provider) {
                uintptr_t* provider_type = *(uintptr_t**)provider;
                /* Each newer constructor stores the injected provider at +4. Their
                 * deleting destructors call only ApplicationD2 and operator delete;
                 * the provider stays our responsibility. Keep unknown layouts owned
                 * by their caller rather than guessing from the pointer alone. */
                bool v2 = provider_type == &vita_audio_provider_vtable_v2[2] &&
                    ((__code_matches(library,
                          "_ZN11AppDelegateC1EP13AudioProviderP15PurchaseManagerP16AnalyticsManagerP13PlatformUtilsP18AchievementManagerP17ControllerManager",
                          120,
                          UINT64_C(0xcd29a864e7888b61)) &&
                         __code_matches(library, "_ZN11AppDelegateD0Ev", 16, UINT64_C(0xba19ee62fe32097a))) ||
                        (__code_matches(library,
                             "_ZN11AppDelegateC1EP13AudioProviderP15PurchaseManagerP16AnalyticsManagerP13PlatformUtilsP18AchievementManagerP17ControllerManager",
                             120,
                             UINT64_C(0xad23492762463dcb)) &&
                            __code_matches(library, "_ZN11AppDelegateD0Ev", 16, UINT64_C(0x3ef069e51ed50412))));
                bool v3 = provider_type == &vita_audio_provider_vtable_v3[2] &&
                    __code_matches(library,
                        "_ZN11AppDelegateC1EP13AudioProviderP15PurchaseManagerP16AnalyticsManagerP13PlatformUtilsP18AchievementManagerP17ControllerManagerP13OnlineManager",
                        120,
                        UINT64_C(0x788635efd26f51df)) &&
                    __code_matches(library, "_ZN11AppDelegateD0Ev", 16, UINT64_C(0x4e2769c6f687d0bb));
                owns_provider = v2 || v3;
            }

            delete_application(*application);
            if(owns_provider) {
                audio_provider_delete(provider);
                log_i(TAG, "Destroyed Vita AudioProvider during game unload");
            }
        } else {
            log_w(TAG, "Leaving application with an unsupported concrete vtable to its owner");
        }
    }

    /* ObjectFactory owns the registered TInfo records and shared strings. Its
     * public destroy API handles both a live singleton and a null instance. */
    void (*destroy_factory)(void) = loader_get_proc_address(library, "_ZN7cocos2d13ObjectFactory15destroyInstanceEv");
    if(destroy_factory) {
        destroy_factory();
    }

    return true;
}

void bridge_patch_cocos2dx(dynalib_t* library)
{
    // cocos2d::experimental::FrameBuffer::_frameBuffers
    // read nullptr
    // lpPatchAddress = malloc(4);
    // patch_uint32(hSoLibrary, 0xA5F750, lpPatchAddress);

    // cocos2d::JniHelper::deleteLocalRefs [BLX LR]
    // __next_prime underflow exception in unordered_map
    // patch_arm(hSoLibrary, 0x393B68, 0xE12FFF3E);

    // cocos2d::JniHelper::convert
    // unordered_map crashed
    // patch_thumb(hSoLibrary, 0x5C2848 + 0, 0x00BF);
    // patch_thumb(hSoLibrary, 0x5C2848 + 2, 0x00BF);
    // patch_thumb(hSoLibrary, 0x5C284C + 0, 0x00BF);
    // patch_thumb(hSoLibrary, 0x5C284E + 0, 0x00BF);
    // patch_thumb(hSoLibrary, 0x5C2850 + 0, 0x00BF);
    // patch_thumb(hSoLibrary, 0x5C2850 + 2, 0x00BF);

    if(library) {
        library->prepare = __cleanup_cocos;
    }
}
