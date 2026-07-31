#ifndef __MOCK_PSP2_IO_STAT_H
#define __MOCK_PSP2_IO_STAT_H
#include <stdint.h>
#include <sys/stat.h>
#include <psp2/types.h>
typedef struct SceIoStat {
    uint32_t st_mode;
    uint32_t st_attr;
    int64_t st_size;
    /* Keep host sys/stat's st_{a,m,c}time macros usable with this mock. */
    struct {
        SceDateTime tv_sec;
    } st_ctim, st_atim, st_mtim;

    uint32_t st_private[6];
} SceIoStat;

#define SCE_CST_SIZE 4
#define SCE_S_IFREG 0020000
#define SCE_S_IFDIR 0010000
#define SCE_S_IFLNK 0040000
#define SCE_S_ISREG(mode) (((mode) & 0170000) == SCE_S_IFREG)
#define SCE_S_ISDIR(mode) (((mode) & 0170000) == SCE_S_IFDIR)
#define SCE_S_ISLNK(mode) (((mode) & 0170000) == SCE_S_IFLNK)
int sceIoGetstat(const char* path, SceIoStat* status);
int sceIoGetstatByFd(int fd, SceIoStat* status);
int sceIoChstatByFd(int fd, const SceIoStat* status, unsigned int fields);
int sceIoMkdir(const char* path, unsigned int mode);
int sceIoRmdir(const char* path);
#endif
