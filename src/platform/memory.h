#ifndef VITAARC_PLATFORM_MEMORY_H
#define VITAARC_PLATFORM_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize graphics with bounded CPU memory pools.
 */
bool vita_memory_setup(void);

void vita_memory_log(const char* stage);
void vita_memory_tick(uint64_t now);

#endif
