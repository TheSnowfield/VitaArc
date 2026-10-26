#include <stdio.h>
#include <string.h>
#include <kubridge.h>

#include <config.h>
#include <loader.h>
#include <logcat/logcat.h>
#include "cocos2dx.h"
#include "texture_size.h"

#define TEXTURE_FORMAT "_ZN7cocos2d9Texture2D13initWithImageEPNS_5ImageENS0_11PixelFormatE"

typedef struct {
    uint32_t size;
    uint64_t hash;
    bool cow;
    uint32_t entry;
    uint32_t init;
    uint32_t alpha;
    uint8_t pixels;
    uint8_t size_offset;
    uint8_t logical;
} texture_abi_t;

static const texture_abi_t __abis[] = {
    {312, UINT64_C(0x10c7e4b37ca4775b), true},  /* 1.6.1c */
    {360, UINT64_C(0x712ed9195bd78633), false}, /* 1.9.0c */
    {360, UINT64_C(0x5a439d00ce7581be), false}, /* 2.0.0c */
    {284, UINT64_C(0x461b928b7f09d5b3), false}, /* 3.0.0c */
    {404, UINT64_C(0x26c11f87daf4503a), false,
        0xa38598, 0xf4bb04, 0x119f10c, 20, 40, 12}, /* 7.0.260c */
};

static bool __cow;
static bool (*__init)(void*, void*, int);
static uint8_t __pixels;
static uint8_t __size;
static uint8_t __logical;

/* The verified prologue runs here, then returns to the original body. Keep this
 * in executable memory; kubridge writes it before Cocos starts any threads. */
__attribute__((naked, aligned(4))) static void __texture_trampoline(void)
{
    __asm__ volatile(
        "nop\n nop\n nop\n nop\n"
        "ldr.w pc, [pc, #0]\n"
        ".word 0\n"
        ".word 0\n");
}

static Elf32_Sym* __symbol(dynalib_t* library, const char* name)
{
    for(uint32_t index = 0; index < library->nsyms; ++index) {
        Elf32_Sym* symbol = &library->symbols[index];
        if(symbol->st_shndx == SHN_UNDEF || ELF32_ST_TYPE(symbol->st_info) != STT_FUNC) {
            continue;
        }

        if(!strcmp((const char*)library->strings + symbol->st_name, name)) {
            return symbol;
        }
    }

    return NULL;
}

static const unsigned char* __code(dynalib_t* library, const char* name, uint32_t size)
{
    Elf32_Sym* symbol = __symbol(library, name);
    if(!symbol || symbol->st_size != size) {
        return NULL;
    }

    uintptr_t address = (uintptr_t)loader_get_proc_address(library, name) & ~(uintptr_t)1;
    if(address < library->base || address - library->base >= library->span) {
        return NULL;
    }

    if(size > library->span - (address - library->base)) {
        return NULL;
    }

    return (const void*)address;
}

static bool __getter(dynalib_t* library, const char* name, uint16_t load)
{
    const uint16_t* code = (const void*)__code(library, name, 4);
    return code && code[0] == load && code[1] == 0x4770;
}

static const unsigned char* __bytes(dynalib_t* library, uint32_t offset, uint32_t size)
{
    for(unsigned index = 0; index < library->header->e_phnum; ++index) {
        const Elf32_Phdr* segment = &library->phdr[index];
        if(segment->p_type != PT_LOAD || !(segment->p_flags & PF_X) || offset < segment->p_vaddr) {
            continue;
        }

        uint32_t relative = offset - segment->p_vaddr;
        if(relative <= segment->p_filesz && size <= segment->p_filesz - relative) {
            return (const void*)(library->base + offset);
        }
    }

    return NULL;
}

static uint64_t __hash(const unsigned char* code, uint32_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for(uint32_t byte = 0; byte < size; ++byte) {
        hash = (hash ^ code[byte]) * UINT64_C(1099511628211);
    }

    return hash;
}

static const texture_abi_t* __layout(dynalib_t* library)
{
    for(unsigned index = 0; index < sizeof(__abis) / sizeof(__abis[0]); ++index) {
        const texture_abi_t* abi = &__abis[index];
        const unsigned char* code;
        if(abi->entry) {
            const unsigned char* entry = __bytes(library, abi->entry, 16);
            if(!entry || __hash(entry, 16) != UINT64_C(0xd61144a4327b9542)) {
                continue;
            }

            code = __bytes(library, abi->init, abi->size);
        } else {
            if(!__getter(library, "_ZNK7cocos2d9Texture2D13getPixelsWideEv", 0x68c0) ||
                !__getter(library, "_ZNK7cocos2d9Texture2D13getPixelsHighEv", 0x6900) ||
                !__getter(library, "_ZN7cocos2d9Texture2D22getContentSizeInPixelsEv", 0x3020)) {
                continue;
            }

            code = __code(library, TEXTURE_FORMAT, abi->size);
        }

        if(!code) {
            continue;
        }

        if(__hash(code, abi->size) == abi->hash) {
            return abi;
        }
    }

    return NULL;
}

static const char* __image_path(const void* image)
{
    /* The verified initWithImage body copies Image::_filePath at +172.
     * GNU COW strings store a pointer; NDK strings use the low-bit SSO tag. */
    const unsigned char* text = (const unsigned char*)image + 172;
    if(__cow) {
        return *(const char* const*)text;
    }

    if(text[0] & 1) {
        return *(const char* const*)(text + 8);
    }

    return (const char*)text + 1;
}

static bool __source_size(const char* name, unsigned packed_width, unsigned packed_height,
    unsigned* width, unsigned* height)
{
    if(!name || !*name) {
        return false;
    }

    size_t prefix = strlen(PATH_TO_PACKAGE);
    if(!strncmp(name, PATH_TO_PACKAGE, prefix)) {
        name += prefix;
    } else if(!strncmp(name, "file:///android_asset/", 22)) {
        name += 22;
    }

    /* Only inspect immutable selected assets, never a generated text bitmap
     * or an unrelated path supplied by the game. */
    if(*name == '/' || strchr(name, ':') || strchr(name, '\\')) {
        return false;
    }

    for(const char* part = name; *part;) {
        const char* slash = strchr(part, '/');
        size_t size = slash ? (size_t)(slash - part) : strlen(part);
        if(!size || (size == 1 && *part == '.') || (size == 2 && !strncmp(part, "..", 2))) {
            return false;
        }

        if(!slash) {
            break;
        }

        part = slash + 1;
    }

    char path[VITAARC_PATH_CAPACITY];
    int size = !strncmp(name, "assets/", 7)
        ? snprintf(path, sizeof(path), "%s%s", PATH_TO_PACKAGE, name)
        : snprintf(path, sizeof(path), "%s/%s", PATH_TO_ASSETS, name);
    if(size < 0 || (size_t)size >= sizeof(path)) {
        return false;
    }

    FILE* file = fopen(path, "rb");
    if(!file) {
        return false;
    }

    unsigned char header[57];
    size_t count = fread(header, 1, sizeof(header), file);
    fclose(file);
    return vita_texture_source_size(header, count, packed_width, packed_height, width, height);
}

static bool __from_image_with_format(void* texture, void* image, int format)
{
    if(!__init(texture, image, format)) {
        return false;
    }

    uint32_t* pixels = (void*)((unsigned char*)texture + __pixels);
    unsigned width, height;
    if(!__source_size(__image_path(image), pixels[0], pixels[1], &width, &height)) {
        return true;
    }

    /* GPU storage and normalized UV limits retain the smaller image size.
     * Sprite rectangles and layout coordinates retain the APK's dimensions. */
    float* size = (void*)((unsigned char*)texture + __size);
    if(__logical) {
        /* Preserve Cocos's /1080/ layout scale while restoring source dimensions. */
        float x = (float)width / pixels[0];
        float y = (float)height / pixels[1];
        float* logical = (void*)((unsigned char*)texture + __logical);
        logical[0] *= x;
        logical[1] *= y;
        size[0] *= x;
        size[1] *= y;
    } else {
        size[0] = width;
        size[1] = height;
    }

    pixels[0] = width;
    pixels[1] = height;
    return true;
}

bool bridge_cocos_configure_textures(dynalib_t* library)
{
    bridge_cocos_release_textures();
    const texture_abi_t* abi = __layout(library);
    if(!abi) {
        log_e(TAG, "Texture profile requires a supported Cocos image ABI");
        bridge_cocos_release_textures();
        return false;
    }

    __cow = abi->cow;
    __pixels = abi->pixels ? abi->pixels : 12;
    __size = abi->size_offset ? abi->size_offset : 32;
    __logical = abi->logical;

    uintptr_t entry = (uintptr_t)loader_get_proc_address(library, TEXTURE_FORMAT);
    if(abi->entry) {
        entry = (library->base + abi->init) | 1;
    }

    if(!(entry & 1) || (entry & 2)) {
        log_e(TAG, "Texture profile requires a supported Cocos image ABI");
        bridge_cocos_release_textures();
        return false;
    }

    const unsigned char* original = (const void*)(entry & ~(uintptr_t)1);
    uint32_t trampoline[5] = {0, 0, 0xf000f8df, entry + 8, 0};
    memcpy(trampoline, original, 8);
    if(abi->cow) {
        /* 1.6.1c loads a PC-relative GOT displacement in its first 8 bytes.
         * Relocate that LDR literal; all other fingerprinted prologues are
         * complete push/add/sub instructions that can be copied unchanged. */
        uint16_t load;
        memcpy(&load, (unsigned char*)trampoline + 6, sizeof(load));
        if(load != 0x4f4b) {
            bridge_cocos_release_textures();
            return false;
        }

        load = 0x4f02; /* LDR r7, [PC, #8] => trampoline+16. */
        memcpy((unsigned char*)trampoline + 6, &load, sizeof(load));
        memcpy(&trampoline[4], original + 0x134, 4);
    }

    void* target = (void*)((uintptr_t)__texture_trampoline & ~(uintptr_t)1);
    if(kuKernelCpuUnrestrictedMemcpy(target, trampoline, sizeof(trampoline)) < 0) {
        bridge_cocos_release_textures();
        return false;
    }

    kuKernelFlushCaches(target, sizeof(trampoline));
    __init = (void*)((uintptr_t)target | 1);
    /* Hook the explicit-format overload too: asynchronous TextureCache loads
     * call it directly. The default overload continues to supply its format. */
    uint32_t hook[2] = {0xf000f8df, (uintptr_t)__from_image_with_format};
    if(kuKernelCpuUnrestrictedMemcpy((void*)original, hook, sizeof(hook)) < 0) {
        bridge_cocos_release_textures();
        return false;
    }

    log_i(TAG, "Vita textures: embedded source sizes; min edge 64, long edge 256, half size up to 960");
    return true;
}

void bridge_cocos_release_textures(void)
{
    __init = NULL;
}
