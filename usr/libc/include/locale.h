/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_LOCALE_H
#define P9_LIBC_LOCALE_H
#include <sys/cdefs.h>
#define LC_ALL 0
#define LC_COLLATE 1
#define LC_CTYPE 2
#define LC_MONETARY 3
#define LC_NUMERIC 4
#define LC_TIME 5
#define LC_MESSAGES 6
struct lconv {
  char *decimal_point, *thousands_sep, *grouping;
  char *int_curr_symbol, *currency_symbol, *mon_decimal_point;
  char *mon_thousands_sep, *mon_grouping, *positive_sign, *negative_sign;
  char int_frac_digits, frac_digits, p_cs_precedes, p_sep_by_space;
  char n_cs_precedes, n_sep_by_space, p_sign_posn, n_sign_posn;
  char int_p_cs_precedes, int_p_sep_by_space, int_n_cs_precedes, int_n_sep_by_space;
  char int_p_sign_posn, int_n_sign_posn;
};
#ifndef __LOCALE_T_DECLARED
typedef struct _locale *locale_t;
#define __LOCALE_T_DECLARED
#endif
__BEGIN_DECLS
char *setlocale(int, const char *);
struct lconv *localeconv(void);
struct lconv *localeconv_l(locale_t);
__END_DECLS
#endif
