/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_TIME_H
#define P9_LIBC_SYS_TIME_H
#include <sys/select.h>
int gettimeofday(struct timeval *, void *) __asm__("p9_gettimeofday");
#endif
