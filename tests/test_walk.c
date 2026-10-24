#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libc/guest_errno.h>
#include "paths.h"

static unsigned __files, __links, __directories;
static int __stop;

static int __count(const char* path, const bridge_bionic_stat_t* status, int type, bridge_walk_info_t* info)
{
    assert(info->base >= 0 && (size_t)info->base <= strlen(path));
    assert(info->level >= 0 && info->level <= 2);
    switch(type) {
        case 0:
            assert(S_ISREG(status->mode) && status->size == 4);
            ++__files;
            break;

        case 3:
            assert(S_ISDIR(status->mode));
            ++__directories;
            break;

        case 5:
            assert(S_ISLNK(status->mode));
            ++__links;
            break;

        default: assert(0);
    }

    return __stop;
}

static int __remove(const char* path, const bridge_bionic_stat_t* status, int type, bridge_walk_info_t* info)
{
    (void)status;
    (void)info;
    return type == 3 ? bridge_guest_rmdir(path) : bridge_guest_unlink(path);
}

int main(void)
{
    test_dirs(PATH_TO_DATA "walk/branch/");
    FILE* file = bridge_fopen("/data/data/moe.low.arc/walk/branch/file", "wb");
    assert(file && fwrite("data", 1, 4, file) == 4 && bridge_fclose(file) == 0);
    assert(symlink("branch", PATH_TO_DATA "walk/link") == 0);
    assert(bridge_guest_nftw("/data/data/moe.low.arc/walk", __count, 1, 5) == 0);
    assert(__files == 1 && __links == 1 && __directories == 2);
    __stop = 73;
    assert(bridge_guest_nftw("/data/data/moe.low.arc/walk", __count, 1, 5) == 73);
    assert(bridge_guest_nftw("/data/data/moe.low.arc/walk", __count, 1, 8) == -1 && errno == 95);
    assert(bridge_guest_nftw("/data/data/moe.low.arc/walk", __remove, 1, 5) == 0);
    assert(access(PATH_TO_DATA "walk", F_OK) == -1 && errno == ENOENT);
    return 0;
}
