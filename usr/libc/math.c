/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <math.h>

double __ieee754_sqrt(double);
double __ieee754_atan2(double, double);

double sqrt(double x) { return __ieee754_sqrt(x); }
double atan2(double y, double x) { return __ieee754_atan2(y, x); }

int
fegetround(void)
{
  unsigned short control;
  __asm__ volatile("fnstcw %0" : "=m"(control));
  return control & 0xc00;
}

int
fesetround(int mode)
{
  unsigned short control;

  if(mode & ~0xc00)
    return -1;
  __asm__ volatile("fnstcw %0" : "=m"(control));
  control = (control & ~0xc00) | mode;
  __asm__ volatile("fldcw %0" : : "m"(control));
  return 0;
}

int
__p9_flt_rounds(void)
{
  static const int modes[] = {1, 3, 2, 0};

  return modes[fegetround() >> 10];
}

double
scalbn(double x, int n)
{
  extern double _scalbln(double, long);

  return _scalbln(x, n);
}
