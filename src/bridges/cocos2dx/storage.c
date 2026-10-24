#include <errno.h>
#include <string.h>

#include <config.h>
#include <loader.h>
#include <logcat/logcat.h>
#include <jni/preferences.h>
#include "cocos2dx.h"

#define COCOS_DATABASE ANDROID_FILES_PATH "/st3"

static bool __vfs(dynalib_t* library)
{
    /* Vita lacks POSIX record locks. SQLite's directory locks retain exclusion
     * between connections without changing the database format. */
    void* (*find)(const char*) = loader_get_proc_address(library, "sqlite3_vfs_find");
    int (*install)(void*, int) = loader_get_proc_address(library, "sqlite3_vfs_register");
    if(!find || !install) {
        log_e(TAG, "Game SQLite VFS exports are unavailable");
        return false;
    }

    void* vfs = find("unix-dotfile");
    if(!vfs || install(vfs, 1) != 0 || find(NULL) != vfs) {
        log_e(TAG, "SQLite atomic-directory locking is unavailable");
        return false;
    }

    log_i(TAG, "SQLite uses atomic-directory locking (unix-dotfile)");
    return true;
}

static bool __database(dynalib_t* library)
{
    int (*open)(const char*, void**, int, const char*) = loader_get_proc_address(library, "sqlite3_open_v2");
    int (*close)(void*) = loader_get_proc_address(library, "sqlite3_close");
    const char* (*message)(void*) = loader_get_proc_address(library, "sqlite3_errmsg");
    int (*error)(void*) = loader_get_proc_address(library, "sqlite3_extended_errcode");
    if(!open || !close || !message || !error) {
        log_e(TAG, "Game SQLite database exports are unavailable");
        return false;
    }

    /* DBManager opens this file on a worker which cannot report its exception
     * to main. Check the same VFS and flags before launching that worker. */
    void* db = NULL;
    int result = open(COCOS_DATABASE, &db, 6, NULL);
    if(result != 0) {
        log_e(TAG,
            "Database open failed: %s code=%d extended=%d message=%s",
            PATH_TO_FILES "st3",
            result,
            db ? error(db) : result,
            db ? message(db) : "no handle");
    }

    int closed = db ? close(db) : 0;
    if(closed != 0) {
        log_e(TAG, "Database close failed: code=%d", closed);
    }

    if(result != 0 || closed != 0) {
        return false;
    }

    log_i(TAG, "Database ready: %s", PATH_TO_FILES "st3");
    return true;
}

bool bridge_cocos_configure_storage(dynalib_t* library)
{
    if(!cocos_prepare_files_directory()) {
        int error = errno;
        log_e(TAG, "Data directory unavailable: %s error=%d (%s)", PATH_TO_FILES, error, strerror(error));
        return false;
    }

    if(!__vfs(library)) {
        return false;
    }

    return __database(library);
}
