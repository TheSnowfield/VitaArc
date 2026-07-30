#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <config.h>
#include <logcat/logcat.h>

#include "assetmgr.h"

#define ASSET_PATH_CAPACITY 1024

struct a_asset_manager
{
  const char *root_path;
};

struct a_asset_dir
{
  char **file_names;
  size_t file_count;
  size_t next_index;
};

struct a_asset
{
  FILE *stream;
  char *path;
  off_t length;
  void *buffer;
};

static a_asset_manager_t __asset_manager = {
    .root_path = PATH_TO_ASSETS,
};

static char *__asset_string_duplicate(const char *text)
{
  size_t length = strlen(text) + 1;
  char *copy = malloc(length);
  if (copy)
    memcpy(copy, text, length);
  return copy;
}

static bool __asset_path_is_safe(const char *path)
{
  if (!path || path[0] == '/' || path[0] == '\\' ||
      strchr(path, ':') || strchr(path, '\\'))
    return false;

  const char *component = path;
  while (*component)
  {
    const char *separator = strchr(component, '/');
    size_t component_length =
        separator ? (size_t)(separator - component) : strlen(component);

    if (component_length == 2 &&
        component[0] == '.' && component[1] == '.')
      return false;

    if (!separator)
      break;
    component = separator + 1;
  }

  return true;
}

static bool __asset_build_path(const a_asset_manager_t *manager,
                               const char *relative_path,
                               char path[ASSET_PATH_CAPACITY])
{
  if (!manager || !manager->root_path ||
      !__asset_path_is_safe(relative_path))
    return false;

  int length;
  if (relative_path[0])
    length = snprintf(path, ASSET_PATH_CAPACITY, "%s/%s",
                      manager->root_path, relative_path);
  else
    length = snprintf(path, ASSET_PATH_CAPACITY, "%s",
                      manager->root_path);

  return length >= 0 && length < ASSET_PATH_CAPACITY;
}

static int __asset_name_compare(const void *first, const void *second)
{
  const char *const *first_name = first;
  const char *const *second_name = second;
  return strcmp(*first_name, *second_name);
}

static void __asset_dir_destroy(a_asset_dir_t *asset_dir)
{
  if (!asset_dir)
    return;

  for (size_t index = 0; index < asset_dir->file_count; ++index)
    free(asset_dir->file_names[index]);
  free(asset_dir->file_names);
  free(asset_dir);
}

void a_asset_dir_close(a_asset_dir_t *asset_dir)
{
  __asset_dir_destroy(asset_dir);
}

const char *a_asset_dir_get_next_file_name(a_asset_dir_t *asset_dir)
{
  if (!asset_dir || asset_dir->next_index >= asset_dir->file_count)
    return NULL;

  return asset_dir->file_names[asset_dir->next_index++];
}

void a_asset_dir_rewind(a_asset_dir_t *asset_dir)
{
  if (asset_dir)
    asset_dir->next_index = 0;
}

a_asset_manager_t *a_asset_manager_from_java(void *env,
                                             void *asset_manager)
{
  (void)env;
  (void)asset_manager;
  return &__asset_manager;
}

a_asset_t *a_asset_manager_open(a_asset_manager_t *manager,
                                const char *filename, int mode)
{
  (void)mode;

  char path[ASSET_PATH_CAPACITY];
  if (!filename || !filename[0] ||
      !__asset_build_path(manager, filename, path))
    return NULL;

  FILE *stream = fopen(path, "rb");
  if (!stream)
  {
    log_w(TAG, "Asset not found: %s", path);
    return NULL;
  }

  if (fseek(stream, 0, SEEK_END) != 0)
  {
    fclose(stream);
    return NULL;
  }

  long length = ftell(stream);
  if (length < 0 || fseek(stream, 0, SEEK_SET) != 0)
  {
    fclose(stream);
    return NULL;
  }

  a_asset_t *asset = calloc(1, sizeof(*asset));
  if (!asset)
  {
    fclose(stream);
    return NULL;
  }

  asset->path = __asset_string_duplicate(path);
  if (!asset->path)
  {
    fclose(stream);
    free(asset);
    return NULL;
  }

  asset->stream = stream;
  asset->length = (off_t)length;
  log_v(TAG, "Opened asset \"%s\" (%ld bytes).", filename, length);
  return asset;
}

a_asset_dir_t *a_asset_manager_open_dir(a_asset_manager_t *manager,
                                        const char *directory_name)
{
  char directory_path[ASSET_PATH_CAPACITY];
  if (!directory_name ||
      !__asset_build_path(manager, directory_name, directory_path))
    return NULL;

  DIR *directory = opendir(directory_path);
  if (!directory)
    return NULL;

  a_asset_dir_t *asset_dir = calloc(1, sizeof(*asset_dir));
  if (!asset_dir)
  {
    closedir(directory);
    return NULL;
  }

  size_t file_capacity = 0;
  struct dirent *entry;
  while ((entry = readdir(directory)) != NULL)
  {
    if (strcmp(entry->d_name, ".") == 0 ||
        strcmp(entry->d_name, "..") == 0)
      continue;

    char file_path[ASSET_PATH_CAPACITY];
    int path_length =
        snprintf(file_path, sizeof(file_path), "%s/%s",
                 directory_path, entry->d_name);
    if (path_length < 0 || path_length >= (int)sizeof(file_path))
      continue;

    struct stat status;
    if (stat(file_path, &status) != 0 || !S_ISREG(status.st_mode))
      continue;

    if (asset_dir->file_count == file_capacity)
    {
      size_t next_capacity = file_capacity ? file_capacity * 2 : 16;
      char **file_names =
          realloc(asset_dir->file_names,
                  next_capacity * sizeof(*file_names));
      if (!file_names)
      {
        closedir(directory);
        __asset_dir_destroy(asset_dir);
        return NULL;
      }
      asset_dir->file_names = file_names;
      file_capacity = next_capacity;
    }

    char *file_name = __asset_string_duplicate(entry->d_name);
    if (!file_name)
    {
      closedir(directory);
      __asset_dir_destroy(asset_dir);
      return NULL;
    }

    asset_dir->file_names[asset_dir->file_count++] = file_name;
  }

  closedir(directory);
  if (asset_dir->file_count > 1)
    qsort(asset_dir->file_names, asset_dir->file_count,
          sizeof(*asset_dir->file_names), __asset_name_compare);
  return asset_dir;
}

void a_asset_close(a_asset_t *asset)
{
  if (!asset)
    return;

  if (asset->stream)
    fclose(asset->stream);
  free(asset->buffer);
  free(asset->path);
  free(asset);
}

int a_asset_read(a_asset_t *asset, void *buffer, size_t count)
{
  if (!asset || !asset->stream)
    return -1;
  if (!count)
    return 0;
  if (!buffer)
    return -1;

  size_t request = count > INT_MAX ? INT_MAX : count;
  size_t bytes_read = fread(buffer, 1, request, asset->stream);
  if (!bytes_read && ferror(asset->stream))
    return -1;
  return (int)bytes_read;
}

off_t a_asset_seek(a_asset_t *asset, off_t offset, int whence)
{
  if (!asset || !asset->stream ||
      fseek(asset->stream, (long)offset, whence) != 0)
    return (off_t)-1;

  long position = ftell(asset->stream);
  return position < 0 ? (off_t)-1 : (off_t)position;
}

const void *a_asset_get_buffer(a_asset_t *asset)
{
  if (!asset || !asset->stream)
    return NULL;
  if (asset->buffer)
    return asset->buffer;

  size_t length = (size_t)asset->length;
  void *buffer = malloc(length ? length : 1);
  if (!buffer)
    return NULL;

  long position = ftell(asset->stream);
  if (position < 0 || fseek(asset->stream, 0, SEEK_SET) != 0 ||
      fread(buffer, 1, length, asset->stream) != length)
  {
    free(buffer);
    if (position >= 0)
      fseek(asset->stream, position, SEEK_SET);
    return NULL;
  }

  if (fseek(asset->stream, position, SEEK_SET) != 0)
  {
    free(buffer);
    return NULL;
  }

  asset->buffer = buffer;
  return asset->buffer;
}

off_t a_asset_get_length(a_asset_t *asset)
{
  return asset ? asset->length : 0;
}

off_t a_asset_get_remaining_length(a_asset_t *asset)
{
  if (!asset || !asset->stream)
    return 0;

  long position = ftell(asset->stream);
  if (position < 0 || (off_t)position >= asset->length)
    return 0;
  return asset->length - (off_t)position;
}

int a_asset_open_file_descriptor(a_asset_t *asset, off_t *out_start,
                                 off_t *out_length)
{
  if (!asset || !asset->path || !out_start || !out_length)
    return -1;

  int file_descriptor = open(asset->path, O_RDONLY);
  if (file_descriptor < 0)
    return -1;

  *out_start = 0;
  *out_length = asset->length;
  return file_descriptor;
}

int a_asset_is_allocated(a_asset_t *asset)
{
  return asset && asset->buffer ? 1 : 0;
}
