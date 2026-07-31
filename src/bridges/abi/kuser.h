#ifndef VITAARC_ABI_KUSER_H
#define VITAARC_ABI_KUSER_H

#include <stdbool.h>
#include <stdint.h>
#include "dynalib.h"

int bridge_arm_kuser_cmpxchg(uint32_t old_value, uint32_t new_value, volatile uint32_t* destination);
void bridge_arm_kuser_memory_barrier(void);
bool bridge_patch_arm_kuser(dynalib_t* library);

#endif
