/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_TIME_H
#define P9_LIBC_TIME_H
#include <sys/types.h>
#include <locale.h>
#include <project9/p9abi.h>
int nanosleep(const struct timespec *, struct timespec *);
time_t time(time_t *);
struct tm {
  int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
  int tm_wday, tm_yday, tm_isdst;
  long tm_gmtoff;
  const char *tm_zone;
};
typedef struct state *timezone_t;
struct tm *gmtime_r(const time_t *, struct tm *);
struct tm *localtime_r(const time_t *, struct tm *);
struct tm *gmtime(const time_t *);
struct tm *localtime(const time_t *);
time_t mktime(struct tm *);
time_t mktime_z(timezone_t, struct tm *);
time_t timegm(struct tm *);
time_t timeoff(struct tm *, long);
double difftime(time_t, time_t);
char *asctime(const struct tm *);
char *asctime_r(const struct tm *, char *);
char *ctime(const time_t *);
char *ctime_r(const time_t *, char *);
size_t strftime(char *, size_t, const char *, const struct tm *);
size_t strftime_l(char *, size_t, const char *, const struct tm *, locale_t);
size_t strftime_lz(timezone_t, char *, size_t, const char *, const struct tm *, locale_t);
size_t strftime_z(timezone_t, char *, size_t, const char *, const struct tm *);
extern char *tzname[2];
void tzset(void);
#endif
