/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <time.h>

static char utc[] = "UTC";
char *tzname[2] = {utc, utc};
static struct tm calendar;
static const unsigned char months[] = {31,28,31,30,31,30,31,31,30,31,30,31};

static int
leap(int year)
{
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

struct tm *
gmtime_r(const time_t *clock, struct tm *tm)
{
  long days = *clock / 86400L, seconds = *clock % 86400L;
  int year = 1970, month, rem;

  if(seconds < 0){
    seconds += 86400;
    days--;
  }
  *tm = (struct tm){0};
  tm->tm_wday = (days + 4) % 7;
  if(tm->tm_wday < 0)
    tm->tm_wday += 7;
  while(days < 0)
    days += 365 + leap(--year);
  while(days >= 365 + leap(year)){
    days -= 365 + leap(year);
    year++;
  }
  tm->tm_yday = days;
  for(month = 0; month < 11; month++){
    rem = months[month] + (month == 1 && leap(year));
    if(days < rem)
      break;
    days -= rem;
  }
  tm->tm_year = year - 1900;
  tm->tm_mon = month;
  tm->tm_mday = days + 1;
  tm->tm_hour = seconds / 3600;
  tm->tm_min = seconds / 60 % 60;
  tm->tm_sec = seconds % 60;
  tm->tm_zone = utc;
  return tm;
}

struct tm *gmtime(const time_t *clock) { return gmtime_r(clock, &calendar); }
struct tm *localtime_r(const time_t *clock, struct tm *tm) { return gmtime_r(clock, tm); }
struct tm *localtime(const time_t *clock) { return gmtime(clock); }

time_t
timeoff(struct tm *tm, long offset)
{
  long long full_year, days, seconds;
  int year, month, era, year_in_era, month_day, era_day;
  time_t result;

  full_year = 1900LL + tm->tm_year + tm->tm_mon / 12;
  month = tm->tm_mon % 12;
  if(month < 0){
    month += 12;
    full_year--;
  }
  full_year -= month <= 1;
  if(full_year < -10000000 || full_year > 10000000)
    goto overflow;
  year = full_year;
  // count from march so the leap day is the last day of a year
  era = (year < 0 ? year-399 : year) / 400;
  year_in_era = year - era*400;
  month_day = (153 * (month + (month < 2 ? 10 : -2)) + 2) / 5;
  era_day = year_in_era*365 + year_in_era/4 - year_in_era/100 + month_day;
  days = 146097LL * era + era_day + tm->tm_mday - 1 - 719468;
  seconds = 86400LL*days + 3600LL*tm->tm_hour + 60LL*tm->tm_min + tm->tm_sec - offset;
  if(seconds < LONG_MIN || seconds > LONG_MAX)
    goto overflow;
  result = seconds;
  gmtime_r(&result, tm);
  return result;
overflow:
  errno = EOVERFLOW;
  return -1;
}

time_t timegm(struct tm *tm) { return timeoff(tm, 0); }
time_t mktime(struct tm *tm) { return timegm(tm); }
time_t mktime_z(timezone_t zone, struct tm *tm) { (void)zone; return timegm(tm); }
void tzset(void) {}
double difftime(time_t a, time_t b) { return (double)a - (double)b; }

struct tm *
localtime_rz(timezone_t zone, const time_t *clock, struct tm *tm)
{
  (void)zone;
  return localtime_r(clock, tm);
}
