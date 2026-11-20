/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_COMPAT_REENTRANT_H
#define P9_LIBC_COMPAT_REENTRANT_H

// project9 streams are single threaded
#define FLOCKFILE(fp) ((void)(fp))
#define FUNLOCKFILE(fp) ((void)(fp))
#define rwlock_rdlock(lock) ((void)0)
#define rwlock_wrlock(lock) ((void)0)
#define rwlock_unlock(lock) ((void)0)
#endif
