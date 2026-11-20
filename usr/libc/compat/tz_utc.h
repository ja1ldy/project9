/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_COMPAT_TZ_UTC_H
#define P9_LIBC_COMPAT_TZ_UTC_H
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>

#define __lcl_ptr ((timezone_t)0)
static inline int __lcl_lock(void) { return 0; }
static inline void __lcl_unlock(bool held) { (void)held; }
static inline time_t __lcl_get_monotonic_time(void) { return 0; }
static inline void tzset_unlocked(bool held, bool wall, time_t t)
{
  (void)held;
  (void)wall;
  (void)t;
}

static inline int
snprintf_l(char *s, size_t n, locale_t locale, const char *fmt, ...)
{
  va_list ap;
  int result;

  (void)locale;
  va_start(ap, fmt);
  result = vsnprintf(s, n, fmt, ap);
  va_end(ap);
  return result;
}
#endif
