#ifndef __BRIDGES_COMPAT_COMPAT_H
#define __BRIDGES_COMPAT_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <libc/poll.h>

extern uint8_t bridge_bionic_stdio[1024];
extern FILE* bridge_bionic_stdout;
extern FILE* bridge_bionic_stderr;
extern const uint8_t* bridge_bionic_ctype;
extern const int16_t* bridge_bionic_tolower;
extern const int16_t* bridge_bionic_toupper;

extern const void* bridge_sl_iid_engine;
extern const void* bridge_sl_iid_play;
extern const void* bridge_sl_iid_prefetch_status;
extern const void* bridge_sl_iid_seek;
extern const void* bridge_sl_iid_volume;

void bridge_compat_initialize(void);
void bridge_google_blocking_region_begin(void);
void bridge_google_blocking_region_end(void);
int bridge_system_property_get(const char* name, char* value);
int bridge_sl_create_engine(void* engine, uint32_t option_count, const void* options, uint32_t interface_count,
    const void* interface_ids, const void* interface_required);
int bridge_writev(int fd, const void* vectors, int count);

#endif /* __BRIDGES_COMPAT_COMPAT_H */
