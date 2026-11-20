/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_COMPAT_SETLOCALE_LOCAL_H
#define P9_LIBC_COMPAT_SETLOCALE_LOCAL_H
#include <locale.h>

struct _locale { const void *part_impl[LC_MESSAGES + 1]; };
locale_t _current_locale(void);
struct lconv *localeconv_l(locale_t);
#endif
