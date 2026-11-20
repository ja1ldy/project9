/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_WCHAR_H
#define P9_LIBC_WCHAR_H
#include <stddef.h>
typedef __WINT_TYPE__ wint_t;
typedef struct { unsigned int state; } mbstate_t;
#define WEOF ((wint_t)-1)
#endif
