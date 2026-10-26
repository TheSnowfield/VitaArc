#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define PATH_TO_APP "app0:"
#define VITAARC_PATH_CAPACITY 1024

/* Selected once before loading the game. The defaults support standalone probes. */
extern char PATH_TO_PACKAGE[VITAARC_PATH_CAPACITY];
extern char PATH_TO_LIBRARY[VITAARC_PATH_CAPACITY];
extern char PATH_TO_ASSETS[VITAARC_PATH_CAPACITY];
extern char PATH_TO_OBB[VITAARC_PATH_CAPACITY];
extern char LIBRARY_LIBCOCOS2DCPP[VITAARC_PATH_CAPACITY];

#define PATH_TO_USER "ux0:data/vitaarc/"
#define PATH_TO_VERSIONS PATH_TO_USER "version/"
#define PATH_TO_LOGFILE PATH_TO_USER "boot.log"
#define PATH_TO_DATA PATH_TO_USER "persistent/data/"
#define PATH_TO_FILES PATH_TO_DATA "files/"
#define ANDROID_FILES_PATH "/data/data/moe.low.arc/files"

typedef struct vita_versions {
    char** names;
    unsigned count;
} vita_versions_t;

/* Pass an empty result (or free a previous result first). An absent directory is
 * an empty successful scan. Names sort by strcmp; I/O errors preserve errno. */
bool vita_versions_scan(vita_versions_t* versions);
void vita_versions_free(vita_versions_t* versions);
bool vita_version_select(const char* name);
const char* vita_version_name(void);
bool vita_version_read_metadata(char* version, size_t capacity);

#endif /* __CONFIG_H */
