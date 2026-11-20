/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_CDEFS_H
#define P9_LIBC_SYS_CDEFS_H
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

#define __CONCAT1(x,y) x ## y
#define ___CONCAT(x,y) __CONCAT1(x,y)
#define __P(args) args
#define __weakref_visible static
#define __weak_reference(name) __attribute__((weakref(#name)))
#define __RCSID(s)
#define __SCCSID(s)
#define __COPYRIGHT(s)
#define __strong_alias(alias, sym) __asm__(".globl " #alias "\n.set " #alias "," #sym);
#define __weak_alias(alias, sym) __asm__(".weak " #alias "\n.set " #alias "," #sym);
#define __UNCONST(p) ((void *)(__UINTPTR_TYPE__)(const void *)(p))
#define __arraycount(a) (sizeof(a) / sizeof((a)[0]))
#define __predict_true(x) __builtin_expect(!!(x), 1)
#define __predict_false(x) __builtin_expect(!!(x), 0)
#define __CTASSERT(x) _Static_assert((x), #x)
#define __dead __attribute__((__noreturn__))
#define __unused __attribute__((__unused__))
#define __used __attribute__((__used__))
#define __printflike(f, a) __attribute__((__format__(__printf__, f, a)))
#define __scanflike(f, a) __attribute__((__format__(__scanf__, f, a)))
#define __BEGIN_EXTERN_C __BEGIN_DECLS
#define __END_EXTERN_C __END_DECLS
#define _DIAGASSERT(x) ((void)0)
#endif
