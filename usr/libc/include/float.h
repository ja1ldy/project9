/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_FLOAT_H
#define P9_LIBC_FLOAT_H
#include_next <float.h>
int __p9_flt_rounds(void);
#undef FLT_ROUNDS
#define FLT_ROUNDS (__p9_flt_rounds())
#endif
