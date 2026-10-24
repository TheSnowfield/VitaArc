#ifndef VITAARC_TEST_PATHS_H
#define VITAARC_TEST_PATHS_H

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <config.h>

static void test_dirs(const char* name)
{
    char path[1024];
    assert(strlen(name) < sizeof(path));
    strcpy(path, name);
    for(char* cursor = path; *cursor; ++cursor) {
        if(*cursor != '/') {
            continue;
        }

        *cursor = 0;
        assert(mkdir(path, 0700) == 0 || errno == EEXIST);
        *cursor = '/';
    }
}

#endif
