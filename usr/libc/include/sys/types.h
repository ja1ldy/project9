/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_TYPES_H
#define P9_LIBC_SYS_TYPES_H
#include <stddef.h>
#include <stdint.h>
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
typedef int ssize_t, pid_t;
typedef unsigned int uid_t, gid_t, mode_t, ino_t, dev_t, nlink_t;
#ifdef P9_USER64
typedef int off_t, time_t;
#else
typedef long off_t, time_t;
#endif
#endif
