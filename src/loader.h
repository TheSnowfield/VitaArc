#ifndef __LOADER_H
#define __LOADER_H

#include "dynalib.h"
#include "patcher.h"

/**
 * Register a sorted local symbol library used to bind ELF imports.
 *
 * @param symbol_refs Sorted local symbol references
 * @param symbol_count Number of local symbol references
 */
void loader_register_symbol_ref(const patch_func_t symbol_refs[],
                                uint32_t symbol_count);

/**
 * Load so library
 *
 * @param library_path Path to library
 *
 * @return not 0 success
 */
dynalib_t *loader_load_library(const char *library_path);

/**
 * Init so library
 *
 * @param library Library instance handle
 *
 */
void loader_init_library(dynalib_t *library);

/**
 * Free so library
 *
 * @param library Library instance handle
 *
 * @return void
 */
void loader_free_library(dynalib_t *library);

/**
 * Get proc address by symbol name
 *
 * @param library Library instance handle
 *
 * @param symbol_name Symbol name to retrieve
 *
 * @return not 0 success
 */
void *loader_get_proc_address(dynalib_t *library, const char *symbol_name);

/**
 * Get library image base
 *
 * @param library Library instance handle
 *
 * @return library image base
 */
void *loader_get_library_image_base(dynalib_t *library);

/**
 * Find loaded library by name
 *
 * @param library_name Library name
 *
 * @return not 0 success
 */
dynalib_t *loader_find_library(const char *library_name);

/**
 * Clone a handle
 *
 * @param library Library instance handle
 *
 * @return not 0 success
 */
dynalib_t *loader_clone_handle(dynalib_t *library);

/**
 * Find an empty library slot (Internal use)
 *
 * @return >0 success
 */
int32_t loader_find_empty_slot(void);

/**
 * Relocate virtual address (Internal use)
 *
 * @param library Library instance
 *
 * @return not 0 success
 */
bool loader_load_sections(dynalib_t *library);

/**
 * Print ELF fotmat information (Internal use)
 *
 * @param library Library instance
 *
 * @return not 0 success
 */
void loader_debug_print_elf_table(dynalib_t *library);


#endif /* __LOADER_H */
