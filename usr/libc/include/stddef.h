/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_STDDEF_H
#define P9_LIBC_STDDEF_H
#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#define NULL ((void *)0)
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif
