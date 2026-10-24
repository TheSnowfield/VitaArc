#ifndef VITAARC_LOADER_INTERNAL_H
#define VITAARC_LOADER_INTERNAL_H

#include "../loader.h"

#define MAX_LIBRARY 32
#define LIBRARY_MAX_SPAN 0x18000000u
#define LIBRARY_ADDRESS_LIMIT 0xB0000000u
#define PAGE_SIZE 0x1000u

#ifndef SCE_KERNEL_MEMBLOCK_TYPE_USER_RX
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RX 0x0C20D050
#endif

#define ALIGN_UP(value, alignment) (((value) + ((alignment) - 1)) & ~((alignment) - 1))
#define ALIGN_DOWN(value, alignment) ((value) & ~((alignment) - 1))

static inline uintptr_t loader_addr(const dynalib_t* library, const Elf32_Sym* symbol)
{
    return symbol->st_shndx == SHN_ABS ? symbol->st_value : library->base + symbol->st_value;
}

bool loader_elf_check(dynalib_t* library, uint32_t file_size);
bool loader_bind(dynalib_t* library);
void* loader_lookup(dynalib_t* requester, const char* symbol_name);

#endif
