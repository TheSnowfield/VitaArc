#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <loader.h>
#include <logcat/logcat.h>
#include <patcher.h>
#include "cocos2dx.h"

#define TEXTURE_MAP PATH_TO_PACKAGE "vitaarc-textures.bin"
#define TEXTURE_INIT "_ZN7cocos2d9Texture2D13initWithImageEPNS_5ImageE"
#define TEXTURE_FORMAT "_ZN7cocos2d9Texture2D13initWithImageEPNS_5ImageENS0_11PixelFormatE"
#define TEXTURE_ALPHA "_ZN7cocos2d9Texture2D26getDefaultAlphaPixelFormatEv"

typedef struct {
    char path[256];
    uint16_t width;
    uint16_t height;
    uint16_t small_width;
    uint16_t small_height;
} texture_row_t;

typedef struct {
    uint32_t size;
    uint64_t hash;
    bool cow;
} texture_abi_t;

static const texture_abi_t __abis[] = {
    {312, UINT64_C(0x10c7e4b37ca4775b), true},  /* 1.6.1c */
    {360, UINT64_C(0x712ed9195bd78633), false}, /* 1.9.0c */
    {360, UINT64_C(0x5a439d00ce7581be), false}, /* 2.0.0c */
    {284, UINT64_C(0x461b928b7f09d5b3), false}, /* 3.0.0c */
};

static texture_row_t* __rows;
static uint32_t __count;
static bool __cow;
static bool (*__init)(void*, void*, int);
static int (*__alpha)(void);

_Static_assert(sizeof(texture_row_t) == 264, "Texture manifest row layout");

static bool __valid_row(const texture_row_t* row)
{
    if(!memchr(row->path, 0, sizeof(row->path)) || strncmp(row->path, "assets/", 7)) {
        return false;
    }

    if(!row->width || !row->height || row->width > 4096 || row->height > 4096) {
        return false;
    }

    if(!row->small_width || !row->small_height) {
        return false;
    }

    return row->small_width <= row->width && row->small_height <= row->height;
}

static bool __read_map(FILE* file)
{
    struct {
        char magic[8];
        uint32_t count;
        uint32_t stride;
    } header;

    if(fread(&header, sizeof(header), 1, file) != 1) {
        return false;
    }

    if(memcmp(header.magic, "VARCTX1", 8) || header.count > 4096 || header.stride != sizeof(texture_row_t)) {
        return false;
    }

    if(!header.count) {
        return fgetc(file) == EOF && !ferror(file);
    }

    __rows = calloc(header.count, sizeof(*__rows));
    if(!__rows) {
        return false;
    }

    if(fread(__rows, sizeof(*__rows), header.count, file) != header.count) {
        return false;
    }

    if(fgetc(file) != EOF || ferror(file)) {
        return false;
    }

    for(uint32_t index = 0; index < header.count; ++index) {
        if(!__valid_row(&__rows[index])) {
            return false;
        }

        if(index && strcmp(__rows[index - 1].path, __rows[index].path) >= 0) {
            return false;
        }
    }

    __count = header.count;
    return true;
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

static const texture_abi_t* __layout(dynalib_t* library)
{
    if(!__getter(library, "_ZNK7cocos2d9Texture2D13getPixelsWideEv", 0x68c0)) {
        return NULL;
    }

    if(!__getter(library, "_ZNK7cocos2d9Texture2D13getPixelsHighEv", 0x6900)) {
        return NULL;
    }

    if(!__getter(library, "_ZN7cocos2d9Texture2D22getContentSizeInPixelsEv", 0x3020)) {
        return NULL;
    }

    for(unsigned index = 0; index < sizeof(__abis) / sizeof(__abis[0]); ++index) {
        const texture_abi_t* abi = &__abis[index];
        const unsigned char* code = __code(library, TEXTURE_FORMAT, abi->size);
        if(!code) {
            continue;
        }

        uint64_t hash = UINT64_C(14695981039346656037);
        for(uint32_t byte = 0; byte < abi->size; ++byte) {
            hash = (hash ^ code[byte]) * UINT64_C(1099511628211);
        }

        if(hash == abi->hash) {
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

static const texture_row_t* __find(const char* path)
{
    if(!path) {
        return NULL;
    }

    const char* prefixes[] = {PATH_TO_PACKAGE, "app0:/package/", "file:///android_asset/"};
    for(unsigned index = 0; index < sizeof(prefixes) / sizeof(prefixes[0]); ++index) {
        size_t length = strlen(prefixes[index]);
        if(!strncmp(path, prefixes[index], length)) {
            path += length;
            break;
        }
    }

    char name[256];
    if(strncmp(path, "assets/", 7)) {
        if(snprintf(name, sizeof(name), "assets/%s", path) >= (int)sizeof(name)) {
            return NULL;
        }

        path = name;
    }

    uint32_t low = 0;
    uint32_t high = __count;
    while(low < high) {
        uint32_t middle = low + (high - low) / 2;
        int order = strcmp(path, __rows[middle].path);
        if(!order) {
            return &__rows[middle];
        }

        if(order < 0) {
            high = middle;
            continue;
        }

        low = middle + 1;
    }

    return NULL;
}

static bool __from_image(void* texture, void* image)
{
    if(!__init(texture, image, __alpha())) {
        return false;
    }

    const texture_row_t* row = __find(__image_path(image));
    if(!row) {
        return true;
    }

    uint32_t* pixels = (void*)((unsigned char*)texture + 12);
    if(pixels[0] != row->small_width || pixels[1] != row->small_height) {
        log_e(TAG, "Texture profile mismatch: %s (%ux%u)", row->path, pixels[0], pixels[1]);
        return false;
    }

    /* GPU storage and normalized UV limits retain the smaller image size.
     * Sprite rectangles and layout coordinates retain the APK's dimensions. */
    pixels[0] = row->width;
    pixels[1] = row->height;
    float* size = (void*)((unsigned char*)texture + 32);
    size[0] = row->width;
    size[1] = row->height;
    return true;
}

bool bridge_cocos_configure_textures(dynalib_t* library)
{
    FILE* file = fopen(TEXTURE_MAP, "rb");
    if(!file) {
        if(errno == ENOENT) {
            return true;
        }

        log_e(TAG, "Cannot open texture profile: %s", strerror(errno));
        return false;
    }

    bool valid = __read_map(file);
    fclose(file);
    if(!valid) {
        log_e(TAG, "Invalid texture profile: %s", TEXTURE_MAP);
        bridge_cocos_release_textures();
        return false;
    }

    if(!__count) {
        return true;
    }

    const texture_abi_t* abi = __layout(library);
    uintptr_t entry = (uintptr_t)loader_get_proc_address(library, TEXTURE_INIT);
    __init = loader_get_proc_address(library, TEXTURE_FORMAT);
    __alpha = loader_get_proc_address(library, TEXTURE_ALPHA);
    if(!abi || !__init || !__alpha || !(entry & 1) || (entry & 2) || !__code(library, TEXTURE_INIT, 16)) {
        log_e(TAG, "Texture profile requires a supported Cocos image ABI");
        bridge_cocos_release_textures();
        return false;
    }

    __cow = abi->cow;
    uint32_t offset = (entry & ~(uintptr_t)1) - library->base;
    patch_uint32(library, offset, 0xf000f8df); /* Thumb LDR.W PC, [PC, #0]. */
    patch_uint32(library, offset + 4, (uintptr_t)__from_image);
    log_i(TAG, "Vita texture profile: %u smaller images, original layout dimensions", __count);
    return true;
}

void bridge_cocos_release_textures(void)
{
    free(__rows);
    __rows = NULL;
    __count = 0;
    __init = NULL;
    __alpha = NULL;
}
