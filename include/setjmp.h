/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_SETJMP_H
#define PROJECT9_SETJMP_H
#ifdef P9_USER64
typedef unsigned long jmp_buf[8];
#else
typedef unsigned int jmp_buf[6];
#endif
int setjmp(jmp_buf) __attribute__((returns_twice));
void longjmp(jmp_buf, int) __attribute__((noreturn));
#endif
