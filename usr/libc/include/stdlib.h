/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_STDLIB_H
#define P9_LIBC_STDLIB_H
#include <stddef.h>
#define MB_CUR_MAX 1
int wctomb(char *, wchar_t);
int mbtowc(wchar_t *, const char *, size_t);
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
void *malloc(size_t) __asm__("p9_malloc");
void free(void *);
void *calloc(size_t, size_t), *realloc(void *, size_t);
int atoi(const char *) __asm__("p9_atoi");
double strtod(const char *, char **);
long strtol(const char *, char **, int);
unsigned long strtoul(const char *, char **, int);
int abs(int);
void abort(void) __attribute__((noreturn));
void *reallocarray(void *, size_t, size_t);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
void qsort_r(void *, size_t, size_t, int (*)(const void *, const void *, void *), void *);
char *getenv(const char *);
int setenv(const char *, const char *, int);
int unsetenv(const char *);
int putenv(char *);
void exit(int) __asm__("p9_exit") __attribute__((noreturn));
int atexit(void (*)(void));
#endif
