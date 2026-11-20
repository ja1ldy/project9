/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_WAIT_H
#define P9_LIBC_SYS_WAIT_H
#include <sys/types.h>
#include <project9/wait.h>
pid_t waitpid(pid_t, int *, int) __asm__("p9_waitpid");
pid_t wait(int *) __asm__("p9_wait");
#endif
