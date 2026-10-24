#ifndef VITAARC_PLATFORM_PERF_H
#define VITAARC_PLATFORM_PERF_H

#include <stdint.h>

/**
 * @brief Record render and presentation wall times on the render owner thread.
 */
void vita_perf_frame(uint64_t started, uint64_t rendered, uint64_t presented);

#endif
