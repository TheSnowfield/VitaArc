/* Exercise the actual game's SQLite core/VFS through production libc bridges.
 * Use only a reserved diagnostic database, never the game's st3 file. */
#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <config.h>
#include <loader.h>
#include <logcat/logcat.h>
#include <bridges/compat/compat.h>
#include <bridges/cocos2dx/cocos2dx.h>

static int selected(void* context, int count, char** values, char** columns)
{
    (void)columns;
    if(count == 1 && values[0] && atoi(values[0]) == 37)
        ++*(int*)context;
    return 0;
}

int main(void)
{
    int result = 1;
    log_begin(PATH_TO_LOGFILE);
    bridge_compat_initialize();
    dynalib_t* game = loader_load_library(LIBRARY_LIBCOCOS2DCPP);
    if(!game || !loader_init_library(game) || !bridge_cocos_configure_storage(game))
        goto done;

    int (*open_db)(const char*, void**, int, const char*) = loader_get_proc_address(game, "sqlite3_open_v2");
    int (*execute)(void*, const char*, int (*)(void*, int, char**, char**), void*, char**) =
        loader_get_proc_address(game, "sqlite3_exec");
    const char* (*error)(void*) = loader_get_proc_address(game, "sqlite3_errmsg");
    int (*close_db)(void*) = loader_get_proc_address(game, "sqlite3_close");
    int (*extended_error)(void*) = loader_get_proc_address(game, "sqlite3_extended_errcode");
    if(!open_db || !execute || !error || !close_db || !extended_error)
        goto done;

    void* database = NULL;
    const char* path = ANDROID_FILES_PATH "/__vitaarc_native_sqlite_probe.db";
    int status = open_db(path, &database, 6, NULL);
    sceClibPrintf("VitaArc sqlite probe: open=%d message=%s\n", status, database ? error(database) : "no handle");
    if(status)
        goto close;

    status = execute(database,
        "CREATE TABLE IF NOT EXISTS probe(value INTEGER);DELETE FROM probe;INSERT INTO probe VALUES(37);",
        NULL,
        NULL,
        NULL);
    sceClibPrintf(
        "VitaArc sqlite probe: write=%d extended=%d message=%s\n", status, extended_error(database), error(database));
    if(status)
        goto close;

    void* contender = NULL;
    if(open_db(path, &contender, 2, NULL))
        goto close;

    int locked = execute(database, "BEGIN EXCLUSIVE;", NULL, NULL, NULL);
    int busy = locked ? -1 : execute(contender, "INSERT INTO probe VALUES(99);", NULL, NULL, NULL);
    int rollback = locked ? -1 : execute(database, "ROLLBACK;", NULL, NULL, NULL);
    int contender_closed = close_db(contender);
    sceClibPrintf("VitaArc sqlite probe: lock=%d contender=%d rollback=%d\n", locked, busy, rollback);
    if(locked || busy != 5 || rollback || contender_closed)
        goto close;

    if(close_db(database))
        goto done;

    database = NULL;
    status = open_db(path, &database, 2, NULL);
    if(status)
        goto close;

    int found = 0;
    status = execute(database, "SELECT value FROM probe;", selected, &found, NULL);
    sceClibPrintf("VitaArc sqlite probe: read=%d rows=%d message=%s\n", status, found, error(database));
    if(!status && found == 1)
        result = 0;
close:
    if(database && close_db(database))
        result = 1;
done:
    sceClibPrintf("VitaArc sqlite probe: exit=%d\n", result);
    log_end();
    sceKernelExitProcess(result);
    return result;
}
