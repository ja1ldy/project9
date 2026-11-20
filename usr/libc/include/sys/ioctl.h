/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_IOCTL_H
#define P9_LIBC_SYS_IOCTL_H
#include <project9/devices.h>
#define ioctl project9_ioctl_prototype
#include <project9/ioctl.h>
#undef ioctl
int ioctl(int, unsigned long, ...) __asm__("p9_ioctl");
#endif
