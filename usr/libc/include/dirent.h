/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_DIRENT_H
#define P9_LIBC_DIRENT_H
#include <sys/types.h>
#include <project9/p9records.h>
typedef struct p9_DIR DIR;
struct dirent { ino_t d_ino; char d_name[P9_NAME_MAX + 1]; };
DIR *opendir(const char *);
struct dirent *readdir(DIR *);
int closedir(DIR *);
void rewinddir(DIR *);
#endif
