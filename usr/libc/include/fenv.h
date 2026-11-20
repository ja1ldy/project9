/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_FENV_H
#define P9_LIBC_FENV_H
#define FE_TONEAREST 0
#define FE_DOWNWARD 0x400
#define FE_UPWARD 0x800
#define FE_TOWARDZERO 0xc00
int fegetround(void);
int fesetround(int);
#endif
