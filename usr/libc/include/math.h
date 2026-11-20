/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_MATH_H
#define P9_LIBC_MATH_H
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))
#define isfinite(x) __builtin_isfinite(x)
#define isinf(x) __builtin_isinf(x)
#define isnan(x) __builtin_isnan(x)
#define signbit(x) __builtin_signbit(x)
#define HUGE_VAL (__builtin_huge_val())
#define M_PI 3.14159265358979323846
double sin(double), cos(double), sqrt(double), atan2(double, double);
double scalbn(double, int);
double floor(double), ceil(double), fabs(double);
#endif
