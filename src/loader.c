#define _GNU_SOURCE

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>

#include <elf.h>
#include <kubridge.h>

#include <logcat/logcat.h>
#include <patcher.h>
#include <utils/debug.h>
#include <utils/fs.h>
#include <utils/string.h>
#include <libc/cxx.h>
#include <libc/sync.h>
#include <abi/kuser.h>
#include <audio/provider.h>

#include "kubridge_ext.h"
#include "loader.h"
#include "loader/internal.h"

static uint32_t __count;
static uintptr_t __cursor = FIRST_LIBRARY_BASE;
static dynalib_t* __libs[MAX_LIBRARY];

void* loader_lookup(dynalib_t* requester, const char* symbol_name)
{
    for(uint32_t slot = 0; slot < MAX_LIBRARY; ++slot) {
        dynalib_t* candidate = __libs[slot];
        if(!candidate || candidate == requester || !candidate->symbols) {
            continue;
        }

        for(uint32_t index = 0; index < candidate->nsyms; ++index) {
            Elf32_Sym* symbol = &candidate->symbols[index];
            if(symbol->st_shndx == SHN_UNDEF) {
                continue;
            }

            const char* name = (const char*)candidate->strings + symbol->st_name;
            if(strcmp(name, symbol_name) == 0) {
                return (void*)loader_addr(candidate, symbol);
            }
        }
    }

    return NULL;
}

static void __free_segments(dynalib_t* library)
{
    while(library->block_count > 0) {
        --library->block_count;
        if(library->blocks[library->block_count] > 0) {
            sceKernelFreeMemBlock(library->blocks[library->block_count]);
            library->blocks[library->block_count] = -1;
        }
    }
}

static bool __file_backed_range(const dynalib_t* library, uint32_t address, uint32_t size)
{
    for(uint32_t index = 0; index < library->header->e_phnum; ++index) {
        const Elf32_Phdr* program = &library->phdr[index];
        if(program->p_type == PT_LOAD && address >= program->p_vaddr &&
            (uint64_t)address + size <= (uint64_t)program->p_vaddr + program->p_filesz) {
            return true;
        }
    }

    return false;
}

static uintptr_t __exidx_table(const dynalib_t* library, uint32_t address, uint32_t size, int* count)
{
    if(!size || address % 4 || size % 8 || !__file_backed_range(library, address, size)) {
        return 0;
    }

    *count = (int)(size / 8);
    return library->base + address;
}

uintptr_t loader_find_exidx(uintptr_t pc, int* count)
{
    if(!count) {
        return 0;
    }

    *count = 0;
    pc &= ~(uintptr_t)1;
    for(uint32_t slot = 0; slot < MAX_LIBRARY; ++slot) {
        const dynalib_t* library = __libs[slot];
        if(!library || pc < library->base || pc - library->base > UINT32_MAX) {
            continue;
        }

        uint32_t relative = (uint32_t)(pc - library->base);
        bool executable = false;
        for(uint32_t index = 0; index < library->header->e_phnum; ++index) {
            const Elf32_Phdr* program = &library->phdr[index];
            if(program->p_type == PT_LOAD && (program->p_flags & PF_X) && relative >= program->p_vaddr &&
                (uint64_t)relative < (uint64_t)program->p_vaddr + program->p_filesz) {
                executable = true;
                break;
            }
        }

        if(!executable) {
            continue;
        }

        for(uint32_t index = 0; index < library->header->e_phnum; ++index) {
            const Elf32_Phdr* program = &library->phdr[index];
            if(program->p_type == PT_ARM_EXIDX) {
                return program->p_filesz == program->p_memsz
                    ? __exidx_table(library, program->p_vaddr, program->p_filesz, count)
                    : 0;
            }
        }

        for(uint32_t index = 0; index < library->header->e_shnum; ++index) {
            const Elf32_Shdr* section = &library->shdr[index];
            if(section->sh_type == SHT_ARM_EXIDX && (section->sh_flags & SHF_ALLOC)) {
                return __exidx_table(library, section->sh_addr, section->sh_size, count);
            }
        }

        return 0;
    }

    return 0;
}

dynalib_t* loader_load_library(const char* library_path)
{
    if(!library_path || !util_file_exists(library_path)) {
        log_f(TAG, "Library does not exist: %s", library_path ? library_path : "(null)");
        return NULL;
    }

    const char* library_name = util_get_file_name(library_path);
    if(!library_name || strlen(library_name) >= sizeof(((dynalib_t*)0)->name) ||
        strlen(library_path) >= sizeof(((dynalib_t*)0)->path)) {
        log_e(TAG, "Library path/name is too long or has no filename");
        return NULL;
    }

    dynalib_t* existing = loader_find_library(library_name);
    if(existing) {
        return loader_clone_handle(existing);
    }

    uint32_t file_size = util_get_file_size(library_path);
    log_i(TAG, "ELF image %s: %u bytes", library_name, file_size);
    if(file_size == 0 || file_size > LIBRARY_ADDRESS_LIMIT - FIRST_LIBRARY_BASE) {
        log_f(TAG, "Library is empty: %s", library_path);
        return NULL;
    }

    dynalib_t* library = calloc(1, sizeof(*library));
    if(!library) {
        return NULL;
    }

    library->image_id = -1;
    for(uint32_t index = 0; index < DYNALIB_MAX_SEGMENTS; ++index) {
        library->blocks[index] = -1;
    }

    library->image_id =
        sceKernelAllocMemBlock("elf_image", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, ALIGN_UP(file_size, PAGE_SIZE), NULL);
    if(library->image_id <= 0 || sceKernelGetMemBlockBase(library->image_id, &library->image) < 0) {
        log_f(TAG, "Could not allocate ELF image for %s", library_name);
        goto failed;
    }

    log_i(TAG, "ELF image storage at %p", library->image);

    SceUID file = sceIoOpen(library_path, SCE_O_RDONLY, 0);
    if(file < 0 || sceIoRead(file, library->image, file_size) != (int32_t)file_size) {
        if(file >= 0) {
            sceIoClose(file);
        }

        log_f(TAG, "Could not read %s", library_path);
        goto failed;
    }

    sceIoClose(file);
    log_i(TAG, "ELF image read complete");

    if(!loader_elf_check(library, file_size)) {
        log_f(TAG, "Invalid ARM ELF shared object: %s", library_path);
        goto failed;
    }

    log_i(TAG, "ELF image validated");

    int32_t slot = loader_find_empty_slot();
    if(slot < 0) {
        log_f(TAG, "Too many loaded libraries");
        goto failed;
    }

    library->slot = (uint32_t)slot;
    snprintf(library->path, sizeof(library->path), "%s", library_path);
    snprintf(library->name, sizeof(library->name), "%s", library_name);
    library->base = __cursor;

    __libs[slot] = library;
    ++__count;
    if(!loader_load_sections(library) || !loader_bind(library) || !bridge_patch_arm_kuser(library)) {
        __libs[slot] = NULL;
        --__count;
        goto failed;
    }

    __cursor = ALIGN_UP(library->base + library->span, LIBRARY_BASE_ALIGNMENT);
    log_i(TAG, "Loaded %s at 0x%08X (%u segments)", library->name, library->base, library->block_count);
    loader_flush_caches(library);
    return loader_clone_handle(library);

failed:
    __free_segments(library);
    if(library->image_id > 0) {
        sceKernelFreeMemBlock(library->image_id);
    }

    free(library);
    return NULL;
}

bool loader_init_library(dynalib_t* library)
{
    if(library && library->unloading) {
        return false;
    }

    if(!library || library->initialized) {
        return library != NULL;
    }

    if(library->ctors) {
        void (**functions)(void) = (void*)(library->base + library->ctors->sh_addr);
        uint32_t count = library->ctors->sh_size / sizeof(*functions);
        for(uint32_t index = 0; index < count; ++index) {
            if(functions[index]) {
                functions[index]();
            }
        }
    }

    library->initialized = true;
    return true;
}

void loader_flush_caches(dynalib_t* library)
{
    if(!library) {
        return;
    }

    for(uint32_t index = 0; index < library->header->e_phnum; ++index) {
        Elf32_Phdr* program = &library->phdr[index];
        if(program->p_type == PT_LOAD) {
            kuKernelFlushCaches((void*)(library->base + program->p_vaddr), program->p_memsz);
        }
    }
}

void loader_debug_print_elf_table(dynalib_t* library)
{
    if(!library || !library->image) {
        return;
    }

    Elf32_Ehdr* header = (Elf32_Ehdr*)library->image;
    log_v(TAG,
        "ELF %s: type=%u machine=%u flags=0x%08X ph=%u sh=%u",
        library->name,
        header->e_type,
        header->e_machine,
        header->e_flags,
        header->e_phnum,
        header->e_shnum);
}

void* loader_get_proc_address(dynalib_t* library, const char* symbol_name)
{
    if(!library || library->finalized || !symbol_name) {
        return NULL;
    }

    for(uint32_t index = 0; index < library->nsyms; ++index) {
        Elf32_Sym* symbol = &library->symbols[index];
        const char* candidate = (const char*)library->strings + symbol->st_name;
        if(symbol->st_shndx != SHN_UNDEF && strcmp(candidate, symbol_name) == 0) {
            void* address = (void*)loader_addr(library, symbol);
            log_v(TAG, "Resolved export %s at %p", symbol_name, address);
            return address;
        }
    }

    return NULL;
}

void* loader_get_global_proc_address(const char* symbol_name)
{
    if(!symbol_name) {
        return NULL;
    }

    for(uint32_t index = 0; index < MAX_LIBRARY; ++index) {
        if(__libs[index] && __libs[index]->unloading) {
            continue;
        }

        void* address = loader_get_proc_address(__libs[index], symbol_name);
        if(address) {
            return address;
        }
    }

    return NULL;
}

void* loader_get_library_image_base(dynalib_t* library)
{
    return library ? library->image : NULL;
}

dynalib_t* loader_find_library(const char* library_name)
{
    if(!library_name) {
        return NULL;
    }

    for(uint32_t index = 0; index < MAX_LIBRARY; ++index) {
        if(__libs[index] && strcmp(__libs[index]->name, library_name) == 0) {
            return __libs[index];
        }
    }

    return NULL;
}

void loader_free_library(dynalib_t* library)
{
    if(!library || library->refs == 0) {
        return;
    }

    if(--library->refs > 0) {
        return;
    }

    if(bridge_sync_range_busy(library->base, library->span)) {
        library->refs = 1;
        log_e(TAG, "Library %s still owns active pthread synchronization; unload deferred", library->name);
        return;
    }

    library->unloading = true;

    if(library->prepare) {
        if(!library->prepare(library)) {
            library->unloading = false;
            library->refs = 1;
            log_e(TAG, "Library %s cleanup preparation failed; unload deferred", library->name);
            return;
        }

        library->prepare = NULL;
    }

    if(library->initialized && library->dtors) {
        void (**functions)(void) = (void*)(library->base + library->dtors->sh_addr);
        uint32_t count = library->dtors->sh_size / sizeof(*functions);
        while(count > 0) {
            --count;
            if(functions[count]) {
                functions[count]();
            }
        }
    }

    /* DSOs normally finalize through their fini array. Also drain callbacks from
     * DSOs lacking that call, before any of their code/data can be unmapped. */
    bridge_cxa_finalize_range(library->base, library->span);
    if(library->finish) {
        void (*cleanup)(dynalib_t*) = library->finish;
        library->finish = NULL;
        cleanup(library);
    }

    library->initialized = false;
    library->finalized = true;
    int sync_error = bridge_sync_destroy_range(library->base, library->span);
    if(sync_error) {
        library->refs = 1;
        log_e(TAG, "Library %s pthread cleanup failed (%d); mappings retained", library->name, sync_error);
        return;
    }

    __libs[library->slot] = NULL;
    --__count;
    __free_segments(library);
    if(library->image_id > 0) {
        sceKernelFreeMemBlock(library->image_id);
    }

    free(library);
}

dynalib_t* loader_clone_handle(dynalib_t* library)
{
    if(library && library->unloading) {
        log_e(TAG, "Library %s is pending final cleanup and cannot be reused", library->name);
        return NULL;
    }

    if(library) {
        ++library->refs;
    }

    return library;
}

int32_t loader_find_empty_slot(void)
{
    if(__count >= MAX_LIBRARY) {
        return -1;
    }

    for(uint32_t index = 0; index < MAX_LIBRARY; ++index) {
        if(!__libs[index]) {
            return (int32_t)index;
        }
    }

    return -1;
}
