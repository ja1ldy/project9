/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_MMAN_H
#define P9_LIBC_SYS_MMAN_H
#include <sys/types.h>
#include <project9/p9abi.h>
#define MAP_FAILED ((void *)-1)
void *mmap(void *, size_t, int, int, int, off_t) __asm__("p9_mmap");
int munmap(void *, size_t) __asm__("p9_munmap");
#endif
