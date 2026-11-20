/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_PTY_H
#define P9_LIBC_PTY_H
#include <sys/ioctl.h>
#include <termios.h>
int openpty(int *, int *, char *, const struct termios *, const struct winsize *) __asm__("p9_openpty");
#endif
