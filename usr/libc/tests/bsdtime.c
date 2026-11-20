/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "netbsdtest.h"
#include <errno.h>
#include <string.h>
#include <time.h>

static void
dates(void)
{
  time_t epoch = 951782400, negative = -1;
  struct tm parts;
  char text[64];

  require(gmtime_r(&epoch, &parts) && parts.tm_wday == 2 && parts.tm_yday == 59,
          "calendar leap day");
  require(strftime(text, sizeof(text), "%Y-%m-%d %a %T %Z", &parts) &&
          !strcmp(text, "2000-02-29 Tue 00:00:00 UTC"), "NetBSD strftime calendar");
  require(asctime_r(&parts, text) && !strcmp(text, "Tue Feb 29 00:00:00 2000\n"),
          "NetBSD asctime");
  require(gmtime_r(&negative, &parts) &&
          strftime(text, sizeof(text), "%s", &parts) == 2 && !strcmp(text, "-1"),
          "pre-epoch time");
  parts = (struct tm){ .tm_year = 100, .tm_mon = 1, .tm_mday = 30 };
  require(timegm(&parts) == 951868800 && parts.tm_mon == 2 && parts.tm_mday == 1,
          "timegm normalizes leap month");
  parts.tm_year = 300;
  errno = 0;
  require(timegm(&parts) == -1 && errno == EOVERFLOW, "time32 overflow");
}

int
main(void)
{
  dates();
  return 0;
}
