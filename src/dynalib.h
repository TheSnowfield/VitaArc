#ifndef __DYNALIB_H
#define __DYNALIB_H

#include <types.h>
#include <elf.h>

#define DYNALIB_MAX_SEGMENTS 16

/**
 * @brief A loaded ELF and its retained mappings.
 * @note Finalization precedes unmapping.
 */
typedef struct dynalib_t {
    /* Registry handle. */
    uint32_t refs;
    uint32_t slot;
    char name[64];
    char path[128];

    /* Metadata image and mapped segments. */
    void* image;
    SceUID image_id;
    uintptr_t base;
    uint32_t span;
    SceUID blocks[DYNALIB_MAX_SEGMENTS];
    uint32_t block_count;
    bool initialized;
    bool unloading;
    bool finalized;

    /* ELF views into the retained metadata image. */
    Elf32_Ehdr* header;
    Elf32_Phdr* phdr;
    Elf32_Shdr* shdr;
    Elf32_Shdr* symtab;
    Elf32_Shdr* ctors;
    Elf32_Shdr* dtors;

    void* names;
    Elf32_Sym* symbols;
    void* strings;
    uint32_t nsyms;

    /* Code and subsystem cleanup hooks. */
    void* text;

    /**
     * @brief Prepare final release. Failure leaves the module intact for retry.
     */
    bool (*prepare)(struct dynalib_t* library);

    /**
     * @brief Finish cleanup after finalizers, before code and exports are unmapped.
     */
    void (*finish)(struct dynalib_t* library);
    void* context;
} dynalib_t;

#endif /* __DYNALIB_H */
