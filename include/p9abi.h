/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_P9ABI_H
#define PROJECT9_P9ABI_H

#define P9_EPERM 1
#define P9_ENOENT 2
#define P9_EINTR 4
#define P9_EIO 5
#define P9_EBADF 9
#define P9_EAGAIN 11
#define P9_ENOMEM 12
#define P9_EACCES 13
#define P9_EFAULT 14
#define P9_EBUSY 16
#define P9_EEXIST 17
#define P9_ENODEV 19
#define P9_ENOTDIR 20
#define P9_EINVAL 22
#define P9_EMFILE 24
#define P9_ENOTTY 25
#define P9_ENOSPC 28
#define P9_ESPIPE 29
#define P9_EOVERFLOW 75
#define P9_EPIPE 32
#define P9_ERANGE 34
#define P9_ENOSYS 38
#define P9_ENFILE 23
#define P9_ENOTSOCK 88
#define P9_EAFNOSUPPORT 97
#define P9_EADDRINUSE 98
#define P9_EADDRNOTAVAIL 99
#define P9_EISCONN 106
#define P9_ENOTCONN 107
#define P9_ECONNREFUSED 111

#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS

#define POLLIN 0x0001
#define POLLOUT 0x0004
#define POLLERR 0x0008
#define POLLHUP 0x0010
#define POLLNVAL 0x0020
struct pollfd { int fd; short events, revents; };

#define FD_SETSIZE 64
typedef struct { unsigned int bits[FD_SETSIZE / 32]; } fd_set;
#if defined(P9_KERNEL64) || defined(P9_USER64)
struct timeval { int tv_sec, tv_usec; };
struct timespec { int tv_sec, tv_nsec; };
#else
struct timeval { long tv_sec, tv_usec; };
struct timespec { long tv_sec, tv_nsec; };
#endif
struct timezone { int tz_minuteswest, tz_dsttime; };

#define P9_PAGE_SIZE 4096
#define P9_HZ 100
#endif
