#ifndef VITAARC_ASSET_CACHE_H
#define VITAARC_ASSET_CACHE_H

#include <stdbool.h>

/**
 * @return false only when a complete directory snapshot proves the path absent.
 */
bool a_asset_cached(const char* path);
void a_asset_cache_clear(void);

#endif
