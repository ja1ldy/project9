/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_COMPAT_LOCALEDEF_H
#define P9_LIBC_COMPAT_LOCALEDEF_H

typedef struct {
  const char *abmon[12], *mon[12];
  const char *abday[7], *day[7];
  const char *d_t_fmt, *d_fmt, *t_fmt, *t_fmt_ampm, *date_fmt;
  const char *am_pm[2];
} _TimeLocale;
#endif
