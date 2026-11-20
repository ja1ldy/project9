/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_INTTYPES_H
#define P9_LIBC_INTTYPES_H
#include <stdint.h>
typedef __INTMAX_TYPE__ intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;
#define PRIdMAX "jd"
#define PRIuMAX "ju"
#define PRId32 "d"
#define PRIu32 "u"
#define PRIx32 "x"
#endif
