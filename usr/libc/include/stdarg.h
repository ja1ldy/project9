/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_STDARG_H
#define P9_LIBC_STDARG_H
typedef __builtin_va_list va_list;
#define va_start(ap, x) __builtin_va_start(ap, x)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, t) __builtin_va_arg(ap, t)
#define va_copy(dst, src) __builtin_va_copy(dst, src)
#endif
