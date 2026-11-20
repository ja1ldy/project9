/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <strings.h>

int
rand(void)
{
  static uint32_t state = 1;
  state = state * 1103515245U + 12345U;
  return state >> 1;
}

int
strncasecmp(const char *a, const char *b, size_t n)
{
  while(n--){
    int x = tolower((unsigned char)*a++);
    int y = tolower((unsigned char)*b++);
    if(x != y || !x)
      return x - y;
  }
  return 0;
}

int
strcasecmp(const char *a, const char *b)
{
  return strncasecmp(a, b, SIZE_MAX);
}

double
asin(double x)
{
  return atan2(x, sqrt((1 - x) * (1 + x)));
}

double
acos(double x)
{
  return atan2(sqrt((1 - x) * (1 + x)), x);
}

double
hypot(double x, double y)
{
  x = fabs(x);
  y = fabs(y);
  if(isinf(x) || isinf(y))
    return INFINITY;
  if(y > x){
    double tmp = x;
    x = y;
    y = tmp;
  }
  if(x == 0)
    return y;
  y /= x;
  return x * sqrt(1 + y * y);
}

double
cbrt(double x)
{
  if(x == 0 || !isfinite(x))
    return x;
  int scale = 0;
  double value = fabs(x);
  while(value >= 8){
    value *= 0.125;
    scale++;
  }
  while(value < 1){
    value *= 8;
    scale--;
  }
  double guess = 2;
  for(int i = 0; i < 8; i++)
    guess = (2 * guess + value / (guess * guess)) / 3;
  return scalbn(x < 0 ? -guess : guess, scale);
}

// the i386 compiler uses these helpers for wide fixed point arithmetic
uint64_t
__udivdi3(uint64_t n, uint64_t d)
{
  if(!d)
    __builtin_trap();
  uint64_t result = 0;
  for(int bit = 63; bit >= 0; bit--){
    if((n >> bit) >= d){
      n -= d << bit;
      result |= (uint64_t)1 << bit;
    }
  }
  return result;
}

int64_t
__divdi3(int64_t numerator, int64_t denominator)
{
  uint64_t n = numerator < 0 ? 0 - (uint64_t)numerator : (uint64_t)numerator;
  uint64_t d = denominator < 0 ? 0 - (uint64_t)denominator : (uint64_t)denominator;
  uint64_t result = __udivdi3(n, d);
  return (numerator < 0) != (denominator < 0) ? 0 - result : result;
}
