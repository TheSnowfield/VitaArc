#ifndef VITAARC_LIBC_WALK_H
#define VITAARC_LIBC_WALK_H

#include "file.h"

typedef struct bridge_walk_info {
    int base;
    int level;
} bridge_walk_info_t;

typedef int (*bridge_walk_call_t)(const char*, const bridge_bionic_stat_t*, int, bridge_walk_info_t*);

int bridge_nftw(const char* path, bridge_walk_call_t callback, int descriptors, int flags);

#endif
