#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "internal.h"

typedef struct {
    const uint8_t* image;
    uint32_t file_size;
    const Elf32_Ehdr* header;
    const Elf32_Phdr* programs;
    const Elf32_Shdr* sections;
    const char* section_names;
    const Elf32_Shdr* symbols;
    const Elf32_Shdr* strings;
    uint32_t symbol_section;
    uint32_t symbol_count;
} elf_view_t;

static bool __file_range(uint32_t offset, uint32_t size, uint32_t file_size)
{
    return offset <= file_size && size <= file_size - offset;
}

static bool __mapped_range(const Elf32_Phdr* programs, uint32_t count, uint32_t address, uint32_t size)
{
    for(uint32_t index = 0; index < count; ++index) {
        const Elf32_Phdr* program = &programs[index];
        if(program->p_type == PT_LOAD && address >= program->p_vaddr &&
            (uint64_t)address + size <= (uint64_t)program->p_vaddr + program->p_memsz) {
            return true;
        }
    }

    return false;
}

static bool __header_ok(const elf_view_t* view)
{
    if(view->file_size < sizeof(Elf32_Ehdr)) {
        return false;
    }

    Elf32_Ehdr* header = (Elf32_Ehdr*)view->image;
    if(memcmp(header->e_ident, ELFMAG, SELFMAG) != 0) {
        return false;
    }

    if(header->e_ident[EI_CLASS] != ELFCLASS32 || header->e_ident[EI_DATA] != ELFDATA2LSB)
        return false;

    if(header->e_ident[EI_VERSION] != EV_CURRENT || header->e_version != EV_CURRENT)
        return false;

    if(header->e_machine != EM_ARM || header->e_type != ET_DYN)
        return false;

    if(header->e_ehsize != sizeof(*header) || header->e_phentsize != sizeof(Elf32_Phdr) ||
        header->e_shentsize != sizeof(Elf32_Shdr)) {
        return false;
    }

    if(!header->e_phnum || !header->e_shnum)
        return false;

    if(!header->e_shstrndx || header->e_shstrndx >= header->e_shnum)
        return false;

    if(header->e_phoff % _Alignof(Elf32_Phdr) || header->e_shoff % _Alignof(Elf32_Shdr))
        return false;

    if(!__file_range(header->e_phoff, header->e_phnum * sizeof(Elf32_Phdr), view->file_size))
        return false;

    if(!__file_range(header->e_shoff, header->e_shnum * sizeof(Elf32_Shdr), view->file_size))
        return false;

    return true;
}

static bool __segments_ok(const elf_view_t* view)
{
    const Elf32_Ehdr* header = view->header;
    const Elf32_Phdr* programs = view->programs;
    uint32_t file_size = view->file_size;
    uint32_t loads = 0;
    for(uint32_t index = 0; index < header->e_phnum; ++index) {
        const Elf32_Phdr* program = &programs[index];
        if(program->p_type != PT_LOAD) {
            continue;
        }

        if(program->p_filesz > program->p_memsz) {
            return false;
        }

        if(!__file_range(program->p_offset, program->p_filesz, file_size)) {
            return false;
        }

        if((uint64_t)program->p_vaddr + program->p_memsz > LIBRARY_ADDRESS_LIMIT - FIRST_LIBRARY_BASE) {
            return false;
        }

        if(program->p_align > 1) {
            if(program->p_align & (program->p_align - 1))
                return false;

            if((program->p_vaddr - program->p_offset) % program->p_align)
                return false;
        }

        if(!program->p_memsz) {
            continue;
        }

        if(++loads > DYNALIB_MAX_SEGMENTS) {
            return false;
        }

        uint32_t start = ALIGN_DOWN(program->p_vaddr, PAGE_SIZE);
        uint32_t end = ALIGN_UP(program->p_vaddr + program->p_memsz, PAGE_SIZE);
        for(uint32_t previous = 0; previous < index; ++previous) {
            const Elf32_Phdr* other = &programs[previous];
            if(other->p_type != PT_LOAD)
                continue;

            if(!other->p_memsz)
                continue;

            if(start >= ALIGN_UP(other->p_vaddr + other->p_memsz, PAGE_SIZE))
                continue;

            if(ALIGN_DOWN(other->p_vaddr, PAGE_SIZE) >= end)
                continue;

            /* Fixed-address memblocks cannot overlap pages. */
            return false;
        }
    }

    return loads > 0;
}

static bool __sections_ok(elf_view_t* view)
{
    const uint8_t* image = view->image;
    const Elf32_Ehdr* header = view->header;
    const Elf32_Phdr* programs = view->programs;
    const Elf32_Shdr* sections = view->sections;
    uint32_t file_size = view->file_size;
    const Elf32_Shdr* names = &sections[header->e_shstrndx];
    if(names->sh_type != SHT_STRTAB || !names->sh_size || !__file_range(names->sh_offset, names->sh_size, file_size)) {
        return false;
    }

    const char* section_names = (const char*)image + names->sh_offset;
    const Elf32_Shdr *symbols = NULL, *strings = NULL;
    uint32_t symbol_section = 0;
    for(uint32_t index = 0; index < header->e_shnum; ++index) {
        const Elf32_Shdr* section = &sections[index];
        if((section->sh_type != SHT_NOBITS && !__file_range(section->sh_offset, section->sh_size, file_size)) ||
            section->sh_name >= names->sh_size ||
            !memchr(section_names + section->sh_name, 0, names->sh_size - section->sh_name)) {
            return false;
        }

        const char* name = section_names + section->sh_name;
        if(!strcmp(name, ".dynsym")) {
            if(symbols || section->sh_type != SHT_DYNSYM || section->sh_entsize != sizeof(Elf32_Sym) ||
                section->sh_size % sizeof(Elf32_Sym) || !section->sh_size || section->sh_offset % _Alignof(Elf32_Sym)) {
                return false;
            }

            symbols = section;
            symbol_section = index;
        } else if(!strcmp(name, ".dynstr")) {
            if(strings || section->sh_type != SHT_STRTAB || !section->sh_size) {
                return false;
            }

            strings = section;
        } else if(!strcmp(name, ".init_array") || !strcmp(name, ".fini_array")) {
            if(section->sh_size % sizeof(uint32_t) ||
                !__mapped_range(programs, header->e_phnum, section->sh_addr, section->sh_size)) {
                return false;
            }
        }
    }

    if(!symbols || !strings || symbols->sh_link >= header->e_shnum || &sections[symbols->sh_link] != strings) {
        return false;
    }

    view->section_names = section_names;
    view->symbols = symbols;
    view->strings = strings;
    view->symbol_section = symbol_section;
    view->symbol_count = symbols->sh_size / sizeof(Elf32_Sym);
    return true;
}

static bool __symbols_ok(const elf_view_t* view)
{
    const uint8_t* image = view->image;
    const Elf32_Ehdr* header = view->header;
    const Elf32_Shdr* symbols = view->symbols;
    const Elf32_Shdr* strings = view->strings;
    uint32_t symbol_count = symbols->sh_size / sizeof(Elf32_Sym);
    const Elf32_Sym* table = (const void*)(image + symbols->sh_offset);
    for(uint32_t index = 0; index < symbol_count; ++index) {
        const Elf32_Sym* symbol = &table[index];
        if(symbol->st_name >= strings->sh_size ||
            !memchr(image + strings->sh_offset + symbol->st_name, 0, strings->sh_size - symbol->st_name) ||
            (symbol->st_shndx >= header->e_shnum && symbol->st_shndx != SHN_ABS)) {
            return false;
        }
    }

    return true;
}

static bool __rels_ok(const elf_view_t* view)
{
    const uint8_t* image = view->image;
    const Elf32_Ehdr* header = view->header;
    const Elf32_Phdr* programs = view->programs;
    const Elf32_Shdr* sections = view->sections;
    const char* section_names = view->section_names;
    uint32_t symbol_section = view->symbol_section;
    uint32_t symbol_count = view->symbol_count;
    for(uint32_t index = 0; index < header->e_shnum; ++index) {
        const Elf32_Shdr* section = &sections[index];
        const char* name = section_names + section->sh_name;
        if(strcmp(name, ".rel.dyn") && strcmp(name, ".rel.plt")) {
            continue;
        }

        if(section->sh_type != SHT_REL || section->sh_entsize != sizeof(Elf32_Rel) ||
            section->sh_size % sizeof(Elf32_Rel) || section->sh_offset % _Alignof(Elf32_Rel) ||
            section->sh_link != symbol_section) {
            return false;
        }

        const Elf32_Rel* relocations = (const void*)(image + section->sh_offset);
        for(uint32_t entry = 0; entry < section->sh_size / sizeof(Elf32_Rel); ++entry) {
            if(ELF32_R_SYM(relocations[entry].r_info) >= symbol_count ||
                (ELF32_R_TYPE(relocations[entry].r_info) != R_ARM_NONE &&
                    !__mapped_range(programs, header->e_phnum, relocations[entry].r_offset, sizeof(uint32_t)))) {
                return false;
            }
        }
    }

    return true;
}

bool loader_elf_check(dynalib_t* library, uint32_t file_size)
{
    elf_view_t view = {.image = library->image, .file_size = file_size, .header = library->image};

    if(!__header_ok(&view))
        return false;

    /* Offsets can only be dereferenced after the header range checks. */
    view.programs = (const void*)(view.image + view.header->e_phoff);
    view.sections = (const void*)(view.image + view.header->e_shoff);
    if(!__segments_ok(&view))
        return false;

    if(!__sections_ok(&view))
        return false;

    if(!__symbols_ok(&view))
        return false;

    return __rels_ok(&view);
}
