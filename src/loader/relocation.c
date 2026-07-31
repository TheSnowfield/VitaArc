#include <stdlib.h>
#include <string.h>
#include <kubridge.h>
#include <logcat/logcat.h>
#include <audio/provider.h>
#include "internal.h"

void* loader_get_import_proc_address(dynalib_t* library, const char* symbol_name)
{
    if(strcmp(symbol_name, "_ZTV24AudioProviderFMODAndroid") == 0) {
        uintptr_t update = (uintptr_t)loader_get_proc_address(library, "_ZN12AudioManager6updateEf");
        uintptr_t code = update & ~(uintptr_t)1;
        bool executable = false;
        for(unsigned index = 0; index < library->header->e_phnum; ++index) {
            Elf32_Phdr* segment = &library->phdr[index];
            uintptr_t start = library->base + segment->p_vaddr;
            if(segment->p_type != PT_LOAD) {
                continue;
            }

            if(!(segment->p_flags & PF_X)) {
                continue;
            }

            if(code < start || code - start > segment->p_filesz) {
                continue;
            }

            if(10 > segment->p_filesz - (code - start)) {
                continue;
            }

            executable = true;
            break;
        }

        uint64_t fingerprint = UINT64_C(14695981039346656037);
        if(executable) {
            for(unsigned index = 0; index < 10; ++index) {
                fingerprint = (fingerprint ^ ((const uint8_t*)code)[index]) * UINT64_C(1099511628211);
            }

            switch(fingerprint) {
                case UINT64_C(0x8085cfa543753f42): return vita_audio_provider_vtable;
                case UINT64_C(0x45f493c31da57b22): return vita_audio_provider_vtable_v2;
                case UINT64_C(0x489e8f9802c732a2): return vita_audio_provider_vtable_v3;
            }
        }

        log_e(TAG, "Unsupported AudioProvider call layout in %s", library->name);
        return NULL;
    }

    void* target = loader_lookup(library, symbol_name);
    return target ? target : loader_get_bridge_proc_address(symbol_name);
}

bool loader_bind(dynalib_t* library)
{
    uint32_t resolved_relocations = 0;
    uint32_t unresolved_symbols = 0;
    bool* reported = calloc(library->nsyms, sizeof(*reported));
    if(!reported) {
        return false;
    }

    for(uint32_t section_index = 0; section_index < library->header->e_shnum; ++section_index) {
        Elf32_Shdr* section = &library->shdr[section_index];
        const char* section_name = (const char*)library->names + section->sh_name;
        if(strcmp(section_name, ".rel.dyn") != 0 && strcmp(section_name, ".rel.plt") != 0) {
            continue;
        }

        Elf32_Rel* relocations = (Elf32_Rel*)((uintptr_t)library->image + section->sh_offset);
        uint32_t count = section->sh_size / sizeof(*relocations);
        for(uint32_t index = 0; index < count; ++index) {
            uint32_t type = ELF32_R_TYPE(relocations[index].r_info);
            if(type == R_ARM_NONE || type == R_ARM_RELATIVE) {
                continue;
            }

            uint32_t symbol_index = ELF32_R_SYM(relocations[index].r_info);
            if(symbol_index >= library->nsyms) {
                continue;
            }

            Elf32_Sym* symbol = &library->symbols[symbol_index];
            if(symbol->st_shndx != SHN_UNDEF) {
                continue;
            }

            const char* symbol_name = (const char*)library->strings + symbol->st_name;
            uintptr_t target = (uintptr_t)loader_get_import_proc_address(library, symbol_name);
            if(!target) {
                if(ELF32_ST_BIND(symbol->st_info) != STB_WEAK && !reported[symbol_index]) {
                    log_e(TAG, "Unresolved required import: %s", symbol_name);
                    reported[symbol_index] = true;
                    ++unresolved_symbols;
                }

                continue;
            }

            uintptr_t* address = (uintptr_t*)(library->base + relocations[index].r_offset);
            uintptr_t value;
            switch(type) {
                case R_ARM_ABS32:
                    value = *address + target;
                    break;

                case R_ARM_GLOB_DAT:
                case R_ARM_JUMP_SLOT:
                    value = target;
                    break;

                default:
                    log_e(TAG, "Unsupported external relocation %u for %s", type, symbol_name);
                    if(!reported[symbol_index]) {
                        reported[symbol_index] = true;
                        ++unresolved_symbols;
                    }

                    continue;
            }

            if(kuKernelCpuUnrestrictedMemcpy(address, &value, sizeof(value)) < 0) {
                free(reported);
                return false;
            }

            ++resolved_relocations;
        }
    }

    free(reported);
    log_i(TAG, "Resolved %u external relocations", resolved_relocations);
    if(unresolved_symbols > 0) {
        log_f(TAG, "Refusing to start with %u unresolved required imports", unresolved_symbols);
        return false;
    }

    return true;
}
