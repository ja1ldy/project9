/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_POLL_H
#define P9_LIBC_POLL_H
#include <project9/p9abi.h>
typedef unsigned int nfds_t;
int poll(struct pollfd *, nfds_t, int) __asm__("p9_poll");
#endif
