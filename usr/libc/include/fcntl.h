/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_FCNTL_H
#define P9_LIBC_FCNTL_H
#include <project9/fcntl.h>
#define O_CREAT O_CREATE
#define O_ACCMODE 3
int open(const char *, int, ...) __asm__("p9_open");
int fcntl(int, int, ...) __asm__("p9_fcntl");
#endif
