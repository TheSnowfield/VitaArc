#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <config.h>
#include <libc/file.h>
#include <libc/posix.h>
#include "preferences.h"

#define PREFS_PATH PATH_TO_DATA "cocos-preferences.bin"
#define PREFS_TEMP PREFS_PATH ".tmp"
#define PREFS_LIMIT (1024u * 1024u)
#define PREFS_MAX_ENTRIES 4096u

typedef struct preference_entry {
    struct preference_entry* next;
    char* key;
    char* value;
    char type;
} preference_entry_t;

static pthread_mutex_t __preferences_mutex = PTHREAD_MUTEX_INITIALIZER;
static const unsigned char __magic[8] = {'V', 'A', 'R', 'C', 'P', 'R', '1', 0};

static bool __prepare_directory(const char* directory)
{
    char path[1024];
    if(strlen(directory) >= sizeof(path)) {
        errno = ENAMETOOLONG;
        return false;
    }

    strcpy(path, directory);
    for(char* cursor = path + 1; *cursor; ++cursor) {
        if(*cursor != '/') {
            continue;
        }

        *cursor = 0;
        /* Device roots (ux0:) already exist on Vita. */
        if(cursor[-1] != ':' && bridge_mkdir(path, 0777) != 0 && errno != EEXIST) {
            return false;
        }

        struct stat status;
        if(stat(path, &status) != 0 || !S_ISDIR(status.st_mode)) {
            errno = ENOTDIR;
            return false;
        }

        *cursor = '/';
    }

    return true;
}

bool cocos_prepare_data_directory(void)
{
    return __prepare_directory(PATH_TO_DATA);
}

bool cocos_prepare_files_directory(void)
{
    return __prepare_directory(PATH_TO_FILES);
}

static void __free_entries(preference_entry_t* entry)
{
    while(entry) {
        preference_entry_t* next = entry->next;
        free(entry->key);
        free(entry->value);
        free(entry);
        entry = next;
    }
}

static uint32_t __read_u32(const unsigned char* bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void __write_u32(unsigned char* bytes, uint32_t value)
{
    for(unsigned int index = 0; index < 4; ++index) {
        bytes[index] = (unsigned char)(value >> (index * 8));
    }
}

static preference_entry_t* __find_entry(preference_entry_t* entries, const char* key)
{
    for(preference_entry_t* entry = entries; entry; entry = entry->next) {
        if(strcmp(entry->key, key) == 0) {
            return entry;
        }
    }

    return NULL;
}

static bool __read_record(const unsigned char* bytes, size_t size, size_t* offset, preference_entry_t** entries)
{
    if(size - *offset < 9) {
        return false;
    }

    char type = (char)bytes[*offset];
    if(!type || !strchr("ZIFS", type)) {
        return false;
    }

    uint32_t key_length = __read_u32(bytes + *offset + 1);
    uint32_t value_length = __read_u32(bytes + *offset + 5);
    *offset += 9;

    if(key_length > size - *offset) {
        return false;
    }

    if(value_length > size - *offset - key_length) {
        return false;
    }

    if(memchr(bytes + *offset, 0, key_length + value_length)) {
        return false;
    }

    preference_entry_t* entry = calloc(1, sizeof(*entry));
    if(!entry) {
        return false;
    }

    entry->key = malloc((size_t)key_length + 1);
    entry->value = malloc((size_t)value_length + 1);
    if(!entry->key || !entry->value) {
        __free_entries(entry);
        return false;
    }

    memcpy(entry->key, bytes + *offset, key_length);
    entry->key[key_length] = 0;
    memcpy(entry->value, bytes + *offset + key_length, value_length);
    entry->value[value_length] = 0;
    entry->type = type;
    if(__find_entry(*entries, entry->key)) {
        __free_entries(entry);
        return false;
    }

    entry->next = *entries;
    *entries = entry;
    *offset += key_length + value_length;
    return true;
}

static bool __decode_entries(const unsigned char* bytes, size_t size, preference_entry_t** entries)
{
    if(memcmp(bytes, __magic, 8) != 0) {
        return false;
    }

    uint32_t count = __read_u32(bytes + 8);
    if(count > PREFS_MAX_ENTRIES) {
        return false;
    }

    size_t offset = 12;
    for(uint32_t index = 0; index < count; ++index) {
        if(!__read_record(bytes, size, &offset, entries)) {
            return false;
        }
    }

    return offset == size;
}

/* Load the entire bounded file before exposing entries. Corrupt data is never
 * overwritten by setters. Independent readers see the last committed file. */
static bool __load_entries(preference_entry_t** entries)
{
    *entries = NULL;
    FILE* file = fopen(PREFS_PATH, "rb");
    if(!file) {
        if(errno != ENOENT) {
            return false;
        }

        errno = 0;
        return true;
    }

    if(fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    long size = ftell(file);
    if(size < 12 || size > PREFS_LIMIT) {
        fclose(file);
        errno = EIO;
        return false;
    }

    if(fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    unsigned char* bytes = malloc((size_t)size);
    if(!bytes) {
        fclose(file);
        errno = ENOMEM;
        return false;
    }

    bool valid = fread(bytes, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    if(valid) {
        valid = __decode_entries(bytes, (size_t)size, entries);
    }

    free(bytes);

    if(!valid) {
        __free_entries(*entries);
        *entries = NULL;
        errno = EIO;
    }

    return valid;
}

static bool __save_entries(preference_entry_t* entries)
{
    size_t size = 12;
    uint32_t count = 0;
    for(preference_entry_t* entry = entries; entry; entry = entry->next) {
        size += 9 + strlen(entry->key) + strlen(entry->value);
        if(size > PREFS_LIMIT || ++count > PREFS_MAX_ENTRIES) {
            errno = EFBIG;
            return false;
        }
    }

    if(!cocos_prepare_data_directory()) {
        return false;
    }

    errno = 0;
    FILE* file = fopen(PREFS_TEMP, "wb");
    if(!file) {
        return false;
    }

    unsigned char header[12];
    memcpy(header, __magic, 8);
    __write_u32(header + 8, count);
    bool ok = fwrite(header, 1, 12, file) == 12;
    for(preference_entry_t* entry = entries; ok && entry; entry = entry->next) {
        uint32_t key_length = (uint32_t)strlen(entry->key);
        uint32_t value_length = (uint32_t)strlen(entry->value);
        header[0] = (unsigned char)entry->type;
        __write_u32(header + 1, key_length);
        __write_u32(header + 5, value_length);
        ok = fwrite(header, 1, 9, file) == 9 && fwrite(entry->key, 1, key_length, file) == key_length &&
            fwrite(entry->value, 1, value_length, file) == value_length;
    }

    if(fflush(file) != 0) {
        ok = false;
    }

    if(fclose(file) != 0) {
        ok = false;
    }

    if(ok && bridge_rename(PREFS_TEMP, PREFS_PATH) == 0) {
        return true;
    }

    int error = errno ? errno : EIO;
    remove(PREFS_TEMP);
    errno = error;
    return false;
}

char* cocos_preferences_get(const char* key, char* type)
{
    if(!key || !type) {
        errno = EINVAL;
        return NULL;
    }

    pthread_mutex_lock(&__preferences_mutex);
    preference_entry_t* entries = NULL;
    char* result = NULL;
    if(__load_entries(&entries)) {
        errno = 0;
        preference_entry_t* entry = __find_entry(entries, key);
        if(entry) {
            result = strdup(entry->value);
            *type = entry->type;
            if(!result) {
                errno = ENOMEM;
            }
        }
    }

    int error = errno;
    __free_entries(entries);
    pthread_mutex_unlock(&__preferences_mutex);
    errno = error;
    return result;
}

bool cocos_preferences_set(const char* key, char type, const char* value)
{
    if(!key || !value || !type || !strchr("ZIFS", type)) {
        errno = EINVAL;
        return false;
    }

    if(strlen(key) > PREFS_LIMIT || strlen(value) > PREFS_LIMIT) {
        errno = EFBIG;
        return false;
    }

    pthread_mutex_lock(&__preferences_mutex);
    preference_entry_t* entries = NULL;
    bool ok = __load_entries(&entries);
    if(ok) {
        preference_entry_t* entry = __find_entry(entries, key);
        if(!entry) {
            entry = calloc(1, sizeof(*entry));
            if(entry) {
                entry->key = strdup(key);
                entry->next = entries;
                entries = entry;
            }
        }

        char* copy = strdup(value);
        if(!entry || !entry->key || !copy) {
            free(copy);
            errno = ENOMEM;
            ok = false;
        } else {
            free(entry->value);
            entry->value = copy;
            entry->type = type;
            ok = __save_entries(entries);
        }
    }

    int error = errno;
    __free_entries(entries);
    pthread_mutex_unlock(&__preferences_mutex);
    errno = error;
    return ok;
}

bool cocos_preferences_delete(const char* key)
{
    if(!key) {
        errno = EINVAL;
        return false;
    }

    pthread_mutex_lock(&__preferences_mutex);
    preference_entry_t* entries = NULL;
    bool ok = __load_entries(&entries);
    if(ok) {
        preference_entry_t** cursor = &entries;
        while(*cursor && strcmp((*cursor)->key, key) != 0) {
            cursor = &(*cursor)->next;
        }

        if(*cursor) {
            preference_entry_t* removed = *cursor;
            *cursor = removed->next;
            removed->next = NULL;
            __free_entries(removed);
            ok = __save_entries(entries);
        }
    }

    int error = errno;
    __free_entries(entries);
    pthread_mutex_unlock(&__preferences_mutex);
    errno = error;
    return ok;
}
