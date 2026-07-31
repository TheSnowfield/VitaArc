#ifndef __TESTS_MOCK_IO_DIRENT_H
#define __TESTS_MOCK_IO_DIRENT_H
#include <psp2/io/stat.h>
typedef struct SceIoDirent {
    SceIoStat d_stat;
    char d_name[256];
    void* d_private;
    int dummy;
} SceIoDirent;

int sceIoDopen(const char* path);
int sceIoDclose(int fd);
int sceIoDread(int fd, SceIoDirent* entry);
#endif
