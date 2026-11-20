/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_REBOOT_H
#define P9_LIBC_SYS_REBOOT_H

#include <project9/reboot.h>
int reboot(int) __asm__("p9_reboot");

#endif
