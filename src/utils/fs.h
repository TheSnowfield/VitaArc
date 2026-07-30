#ifndef __UTILS_FS_H
#define __UTILS_FS_H

#include <types.h>

bool util_file_exists(const char *file_path);
uint32_t util_get_file_size(const char *file_path);
bool utils_read_file_all(const char *file_path, void *memory,
                         uint32_t buffer_size);

#endif /* __UTILS_FS_H */
