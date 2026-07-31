#include <stdint.h>
#include <string.h>
#include <psp2/kernel/sysmem.h>
#include <kubridge.h>
#include <logcat/logcat.h>
#include "../kubridge_ext.h"
#include "internal.h"

static bool __relocate(uint32_t type, uintptr_t* address, uintptr_t symbol, uintptr_t base)
{
    uintptr_t value;
    switch(type) {
        case R_ARM_NONE: return true;

        case R_ARM_ABS32:
            if(!symbol) {
                return true;
            }

            value = *address + symbol;
            break;

        case R_ARM_RELATIVE:
            value = *address + base;
            break;

        case R_ARM_GLOB_DAT:
        case R_ARM_JUMP_SLOT:
            value = symbol;
            break;

        default:
            log_e(TAG, "Unsupported internal relocation type %u", type);
            return false;
    }

    return kuKernelCpuUnrestrictedMemcpy(address, &value, sizeof(value)) >= 0;
}

bool loader_load_sections(dynalib_t* library)
{
    uintptr_t image = (uintptr_t)library->image;
    Elf32_Ehdr* header = (Elf32_Ehdr*)image;
    Elf32_Phdr* programs = (Elf32_Phdr*)(image + header->e_phoff);
    Elf32_Shdr* sections = (Elf32_Shdr*)(image + header->e_shoff);
    uint32_t image_end = 0;

    for(uint32_t index = 0; index < header->e_phnum; ++index) {
        Elf32_Phdr* program = &programs[index];
        if(program->p_type != PT_LOAD || !program->p_memsz) {
            continue;
        }

        if(library->block_count >= DYNALIB_MAX_SEGMENTS || program->p_filesz > program->p_memsz) {
            return false;
        }

        uintptr_t virtual_start = ALIGN_DOWN(program->p_vaddr, PAGE_SIZE);
        uintptr_t target_start = library->base + virtual_start;
        uint32_t prefix = program->p_vaddr - virtual_start;
        uint32_t block_size = ALIGN_UP(prefix + program->p_memsz, PAGE_SIZE);
        if((uint64_t)library->base + virtual_start + block_size > LIBRARY_ADDRESS_LIMIT) {
            log_e(TAG, "Library address space exhausted");
            return false;
        }

        SceKernelAllocMemBlockKernelOpt options;
        memset(&options, 0, sizeof(options));
        options.size = sizeof(options);
        options.field_C = target_start;
        options.attr = 0x01;
        SceKernelMemBlockType type =
            (program->p_flags & PF_X) ? SCE_KERNEL_MEMBLOCK_TYPE_USER_RX : SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
        SceUID block =
            kuKernelAllocMemBlock((program->p_flags & PF_X) ? "elf_rx" : "elf_rw", type, block_size, &options);
        if(block <= 0) {
            log_e(TAG, "Could not map segment at 0x%08X (kubridge error 0x%08X)", target_start, (uint32_t)block);
            return false;
        }

        library->blocks[library->block_count++] = block;

        void* actual_base = NULL;
        if(sceKernelGetMemBlockBase(block, &actual_base) < 0 || (uintptr_t)actual_base != target_start) {
            log_e(TAG, "Segment mapped at unexpected address %p", actual_base);
            return false;
        }

        if(kuKernelCpuUnrestrictedMemset(actual_base, 0, block_size) < 0 ||
            kuKernelCpuUnrestrictedMemcpy((void*)(library->base + program->p_vaddr),
                (const void*)(image + program->p_offset),
                program->p_filesz) < 0) {
            return false;
        }

        if(program->p_flags & PF_X) {
            library->text = (void*)(library->base + program->p_vaddr);
        }

        uint32_t segment_end = program->p_vaddr + program->p_memsz;
        if(segment_end > image_end) {
            image_end = segment_end;
        }
    }

    library->span = ALIGN_UP(image_end, PAGE_SIZE);

    char* section_names = (char*)(image + sections[header->e_shstrndx].sh_offset);
    Elf32_Sym* dynamic_symbols = NULL;
    char* dynamic_strings = NULL;
    uint32_t dynamic_symbol_count = 0;
    for(uint32_t index = 0; index < header->e_shnum; ++index) {
        const char* name = section_names + sections[index].sh_name;
        if(strcmp(name, ".dynsym") == 0) {
            dynamic_symbols = (Elf32_Sym*)(image + sections[index].sh_offset);
            dynamic_symbol_count = sections[index].sh_size / sizeof(Elf32_Sym);
        } else if(strcmp(name, ".dynstr") == 0) {
            dynamic_strings = (char*)(image + sections[index].sh_offset);
        } else if(strcmp(name, ".init_array") == 0) {
            library->ctors = &sections[index];
        } else if(strcmp(name, ".fini_array") == 0) {
            library->dtors = &sections[index];
        }
    }

    if(!dynamic_symbols || !dynamic_strings) {
        return false;
    }

    for(uint32_t section_index = 0; section_index < header->e_shnum; ++section_index) {
        const char* name = section_names + sections[section_index].sh_name;
        if(strcmp(name, ".rel.dyn") != 0 && strcmp(name, ".rel.plt") != 0) {
            continue;
        }

        Elf32_Rel* relocations = (Elf32_Rel*)(image + sections[section_index].sh_offset);
        uint32_t count = sections[section_index].sh_size / sizeof(Elf32_Rel);
        for(uint32_t index = 0; index < count; ++index) {
            uint32_t symbol_index = ELF32_R_SYM(relocations[index].r_info);
            if(symbol_index >= dynamic_symbol_count) {
                return false;
            }

            Elf32_Sym* symbol = &dynamic_symbols[symbol_index];
            uintptr_t* address = (uintptr_t*)(library->base + relocations[index].r_offset);
            uintptr_t symbol_address = 0;
            if(symbol->st_shndx != SHN_UNDEF) {
                symbol_address = loader_addr(library, symbol);
            }

            uint32_t type = ELF32_R_TYPE(relocations[index].r_info);
            if(!__relocate(type, address, symbol_address, library->base)) {
                return false;
            }
        }
    }

    library->header = header;
    library->phdr = programs;
    library->shdr = sections;
    library->names = section_names;
    library->symbols = dynamic_symbols;
    library->strings = dynamic_strings;
    library->nsyms = dynamic_symbol_count;
    return true;
}
