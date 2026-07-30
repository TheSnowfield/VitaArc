#ifndef __BRIDGES_SYMBOLS_H
#define __BRIDGES_SYMBOLS_H

#include "dynalib.h"

void loader_symbol_ref(void);
void patch_bridge_symbols(dynalib_t *library);
uint32_t bridge_symbol_count(void);

#endif /* __BRIDGES_SYMBOLS_H */
