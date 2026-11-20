/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_FNMATCH_H
#define P9_LIBC_FNMATCH_H
#define FNM_PATHNAME 1
#define FNM_NOESCAPE 2
#define FNM_PERIOD 4
#define FNM_NOMATCH 1
int fnmatch(const char *, const char *, int);
#endif
