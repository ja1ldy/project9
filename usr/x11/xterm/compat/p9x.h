/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#define O_NDELAY O_NONBLOCK
#define EWOULDBLOCK EAGAIN
#define isascii(c) ((unsigned)(c) < 128)
typedef char *caddr_t;
int gethostname(char *, size_t);
int getdtablesize(void);
int putenv(char *);
#include <wchar.h>
#include <limits.h>
typedef long suseconds_t;
#ifndef MB_LEN_MAX
#define MB_LEN_MAX 1
#endif
size_t wcslen(const wchar_t *);
wchar_t *wcscpy(wchar_t *, const wchar_t *);
wchar_t *wcsncpy(wchar_t *, const wchar_t *, size_t);
long labs(long);
double atof(const char *);
#include <stdio.h>
FILE *popen(const char *, const char *);
int pclose(FILE *);
struct _XDisplay;
void _XEatDataWords(struct _XDisplay *, unsigned long);

typedef unsigned int fd_mask;
