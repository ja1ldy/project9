/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_PARAM_H
#define P9_LIBC_SYS_PARAM_H
#include <limits.h>
#include <sys/types.h>
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define ALIGNBYTES (sizeof(long)-1)
#define ALIGN(n) (((n)+ALIGNBYTES) & ~ALIGNBYTES)
#define MAXPATHLEN PATH_MAX
#endif
