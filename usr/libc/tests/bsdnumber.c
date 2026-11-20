/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "netbsdtest.h"
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

static void
numbers(void)
{
  char *end, text[64];
  volatile double x = 0.25, large = 1e30;

  errno = 0;
  require(strtol("-2147483648!", &end, 10) == LONG_MIN && *end == '!' && !errno,
          "strtol exact signed limit");
  require(strtoul("0xff!", &end, 0) == 255 && *end == '!', "strtoul base prefix");
  require(strtod("  -12.5!", &end) == -12.5 && *end == '!', "gdtoa decimal input");
  require(strtod("0x1.8p2", &end) == 6.0 && !*end, "gdtoa hexadecimal input");
  require(isinf(strtod("Inf", &end)) && !*end && isnan(strtod("nan", &end)), "gdtoa special input");
  require(fesetround(FE_UPWARD) == 0 && FLT_ROUNDS == 2 &&
          strtod("1.00000000000000000001", NULL) > 1.0 &&
          fesetround(FE_TONEAREST) == 0, "gdtoa directed rounding");
  require(snprintf(text, sizeof(text), "%+.2f %.2e %.4g", -3.125, 12.5, 1000.0) > 0 &&
          !strcmp(text, "-3.12 1.25e+01 1000"), "gdtoa formatted output");
  require(snprintf(text, 5, "%09.2f", 0.125) == 9 && !strcmp(text, "0000"),
          "decimal formatting truncation");
  require(fabs(sin(x)*sin(x)+cos(x)*cos(x)-1.0) < 1e-12 &&
          fabs(sin(large)) <= 1.0 && fabs(cos(large)) <= 1.0 &&
          sqrt(2.25) == 1.5 && fabs(atan2(1.0,0.0)-M_PI/2) < 1e-12,
          "NetBSD libm");
}

int
main(void)
{
  numbers();
  return 0;
}
