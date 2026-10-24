#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "directory.h"
#include "guest_errno.h"
#include "walk.h"

enum {
    WALK_FILE,
    WALK_DIRECTORY,
    WALK_UNREADABLE,
    WALK_POSTORDER,
    WALK_UNKNOWN,
    WALK_LINK,
    WALK_DANGLING,
    WALK_PHYSICAL = 1,
    WALK_MOUNT = 2,
    WALK_DEPTH = 4,
};

typedef struct walk_name {
    struct walk_name* next;
    char text[];
} walk_name_t;

typedef struct walk_parent {
    const struct walk_parent* parent;
    uint64_t device;
    uint64_t inode;
} walk_parent_t;

typedef struct walk_t {
    bridge_walk_call_t call;
    int flags;
    uint64_t device;
} walk_t;

static int __visit(walk_t* walk, const char* path, const bridge_bionic_stat_t* status, int type, int level)
{
    const char* slash = strrchr(path, '/');
    bridge_walk_info_t info = {.base = slash ? slash - path + 1 : 0, .level = level};
    errno = bridge_errno_from_native(errno);
    int result = walk->call(path, status, type, &info);
    errno = bridge_errno_to_native(errno);
    return result;
}

static void __free_names(walk_name_t* names)
{
    while(names) {
        walk_name_t* next = names->next;
        free(names);
        names = next;
    }
}

static int __names(bridge_directory_t* directory, walk_name_t** names)
{
    walk_name_t** tail = names;
    for(;;) {
        errno = 0;
        bridge_dirent_t* entry = bridge_readdir(directory);
        if(!entry) {
            return errno ? -1 : 0;
        }

        if(!strcmp(entry->name, ".") || !strcmp(entry->name, "..")) {
            continue;
        }

        size_t length = strlen(entry->name);
        walk_name_t* name = malloc(sizeof(*name) + length + 1);
        if(!name) {
            errno = ENOMEM;
            return -1;
        }

        name->next = NULL;
        memcpy(name->text, entry->name, length + 1);
        *tail = name;
        tail = &name->next;
    }
}

static int __walk(walk_t* walk, const char* path, int level, const walk_parent_t* parent)
{
    if(level > 64) {
        errno = ELOOP;
        return -1;
    }

    bridge_bionic_stat_t status = {0};
    if(bridge_bionic_lstat(path, &status)) {
        return level ? __visit(walk, path, &status, WALK_UNKNOWN, level) : -1;
    }

    if(S_ISLNK(status.mode)) {
        if(walk->flags & WALK_PHYSICAL) {
            return __visit(walk, path, &status, WALK_LINK, level);
        }

        if(bridge_bionic_stat(path, &status)) {
            return __visit(walk, path, &status, WALK_DANGLING, level);
        }
    }

    if(!S_ISDIR(status.mode)) {
        return __visit(walk, path, &status, WALK_FILE, level);
    }

    if(!level) {
        walk->device = status.device;
    }

    if((walk->flags & WALK_MOUNT) && status.device != walk->device) {
        return 0;
    }

    for(const walk_parent_t* entry = parent; entry; entry = entry->parent) {
        if(entry->device == status.device && entry->inode == status.inode) {
            return 0;
        }
    }

    bridge_directory_t* directory = bridge_opendir(path);
    if(!directory) {
        return __visit(walk, path, &status, WALK_UNREADABLE, level);
    }

    /* Snapshot names and close the handle before descending. This needs only
     * one directory descriptor, including callers that specify a limit of one. */
    walk_name_t* names = NULL;
    int result = __names(directory, &names);
    int error = errno;
    bridge_closedir(directory);
    errno = error;
    if(!result && !(walk->flags & WALK_DEPTH)) {
        result = __visit(walk, path, &status, WALK_DIRECTORY, level);
    }

    walk_parent_t current = {.parent = parent, .device = status.device, .inode = status.inode};
    for(walk_name_t* name = names; name && !result; name = name->next) {
        char child[1024];
        int length = snprintf(child, sizeof(child), "%s%s%s", path, path[strlen(path) - 1] == '/' ? "" : "/", name->text);
        if(length < 0 || length >= (int)sizeof(child)) {
            errno = ENAMETOOLONG;
            result = -1;
            break;
        }

        result = __walk(walk, child, level + 1, &current);
    }

    __free_names(names);
    if(!result && (walk->flags & WALK_DEPTH)) {
        result = __visit(walk, path, &status, WALK_POSTORDER, level);
    }

    return result;
}

int bridge_nftw(const char* path, bridge_walk_call_t callback, int descriptors, int flags)
{
    if(!path || !*path || !callback || descriptors < 1 || (flags & ~15)) {
        errno = EINVAL;
        return -1;
    }

    if(flags & 8) {
        errno = ENOTSUP;
        return -1;
    }

    walk_t walk = {.call = callback, .flags = flags};
    return __walk(&walk, path, 0, NULL);
}
