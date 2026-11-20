/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_ASSERT_H
#define P9_LIBC_ASSERT_H
#include <sys/cdefs.h>
__BEGIN_DECLS
void __p9_assert(const char *, const char *, int) __dead;
__END_DECLS
#endif

#undef assert
#ifdef NDEBUG
#define assert(x) ((void)0)
#else
#define assert(x) ((x) ? (void)0 : __p9_assert(#x, __FILE__, __LINE__))
#endif
