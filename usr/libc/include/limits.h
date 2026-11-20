/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_LIMITS_H
#define P9_LIBC_LIMITS_H
#define MB_LEN_MAX 1
#define CHAR_BIT 8
#define SCHAR_MIN (-__SCHAR_MAX__-1)
#define SCHAR_MAX __SCHAR_MAX__
#define UCHAR_MAX (2U*SCHAR_MAX+1)
#ifdef __CHAR_UNSIGNED__
#define CHAR_MIN 0
#define CHAR_MAX UCHAR_MAX
#else
#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX
#endif
#define USHRT_MAX (2U*SHRT_MAX+1)
#define SHRT_MAX __SHRT_MAX__
#define SHRT_MIN (-SHRT_MAX-1)
#define _POSIX2_RE_DUP_MAX 255
#define INT_MAX __INT_MAX__
#define INT_MIN (-INT_MAX-1)
#define LONG_MAX __LONG_MAX__
#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-LLONG_MAX-1LL)
#define ULLONG_MAX (2ULL*LLONG_MAX+1)
#define LONG_MIN (-LONG_MAX-1L)
#define UINT_MAX (2U*INT_MAX+1)
#define ULONG_MAX (2UL*LONG_MAX+1)
#define PATH_MAX 512
#define NAME_MAX 14
#endif
