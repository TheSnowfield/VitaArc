#ifndef VITAARC_LIBC_DIRECTORY_H
#define VITAARC_LIBC_DIRECTORY_H

#include <stdint.h>

/* Bionic uses the same 64-bit layout for dirent and dirent64 on ARM32. */
typedef struct bridge_dirent {
    uint64_t inode;
    int64_t offset;
    uint16_t record_length;
    uint8_t type;
    char name[256];
} __attribute__((aligned(8))) bridge_dirent_t;

typedef struct bridge_directory bridge_directory_t;

bridge_directory_t* bridge_opendir(const char* path);
bridge_directory_t* bridge_fdopendir(int fd);
bridge_dirent_t* bridge_readdir(bridge_directory_t* directory);
int bridge_readdir_r(bridge_directory_t* directory, bridge_dirent_t* entry, bridge_dirent_t** result);
int bridge_closedir(bridge_directory_t* directory);
void bridge_rewinddir(bridge_directory_t* directory);
void bridge_seekdir(bridge_directory_t* directory, int32_t position);
int32_t bridge_telldir(bridge_directory_t* directory);
int bridge_dirfd(bridge_directory_t* directory);

#endif
