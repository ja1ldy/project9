/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "compat/setlocale_local.h"
#include "compat/sys/localedef.h"

static char empty[] = "", dot[] = ".", name[] = "C";
static struct lconv conventions = {
  dot, empty, empty, empty, empty, empty, empty, empty, empty, empty,
  CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
  CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX
};
static const _TimeLocale calendar = {
  .abmon = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"},
  .mon = {"January","February","March","April","May","June","July",
          "August","September","October","November","December"},
  .abday = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"},
  .day = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"},
  .d_t_fmt = "%a %b %e %T %Y",
  .d_fmt = "%m/%d/%y",
  .t_fmt = "%H:%M:%S",
  .t_fmt_ampm = "%I:%M:%S %p",
  .date_fmt = "%a %b %e %T %Z %Y",
  .am_pm = {"AM", "PM"}
};
static struct _locale c_locale = { .part_impl[LC_TIME] = &calendar };

char *
setlocale(int which, const char *setting)
{
  if(which < LC_ALL || which > LC_MESSAGES)
    return NULL;
  if(setting && *setting && strcmp(setting, "C") && strcmp(setting, "POSIX"))
    return NULL;
  return name;
}

struct lconv *localeconv(void) { return &conventions; }
struct lconv *localeconv_l(locale_t loc) { (void)loc; return localeconv(); }
locale_t _current_locale(void) { return &c_locale; }

int strcoll(const char *a, const char *b) { return strcmp(a, b); }

int
wctomb(char *dest, wchar_t ch)
{
  if(!dest)
    return 0;
  if((unsigned long)ch > 127)
    return -1;
  *dest = ch;
  return 1;
}

int
mbtowc(wchar_t *dest, const char *source, size_t size)
{
  if(!source)
    return 0;
  if(!size || (unsigned char)*source > 127)
    return -1;
  if(dest)
    *dest = (unsigned char)*source;
  return *source != 0;
}
