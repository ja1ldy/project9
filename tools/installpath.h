/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_INSTALLPATH_H
#define PROJECT9_INSTALLPATH_H

#include <string.h>

// match the source folder and the staged folder the same way
static int
in_directory(const char *path, const char *directory)
{
  const char *base = strrchr(path, '/');
  size_t len = strlen(directory);

  return base && (size_t)(base - path) >= len &&
         strncmp(base - len, directory, len) == 0 &&
         ((size_t)(base - path) == len || *(base - len - 1) == '/');
}

static int
is_manual_path(const char *path)
{
  return in_directory(path, "usr/share/man") || in_directory(path, "etc/man");
}

#endif
