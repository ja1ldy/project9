/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_X_COMPAT_H
#define P9_X_COMPAT_H
#include <stdint.h>
#include <strings.h>
#define UINT16_MAX __UINT16_MAX__
#define UINT32_C(n) __UINT32_C(n)
#define UINT64_C(n) __UINT64_C(n)
int rand(void);
double asin(double);
double acos(double);
double hypot(double, double);
#endif
