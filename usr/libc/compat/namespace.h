/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_COMPAT_NAMESPACE_H
#define P9_LIBC_COMPAT_NAMESPACE_H

#include <sys/cdefs.h>

// use the public names directly in the static library
#undef __weak_alias
#define __weak_alias(alias, sym)
#endif
