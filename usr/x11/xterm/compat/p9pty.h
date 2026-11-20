/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <setjmp.h>
typedef jmp_buf sigjmp_buf;
#define siglongjmp longjmp
#define ENXIO ENODEV
#define ECHOE 0
#define ECHOK 0
#define INLCR 0
#define IGNCR 0
#define CSIZE 0
#define CS7 0
#define CS8 0
#define TAB3 0
#define B0 0
#define B50 50
#define B75 75
#define B110 110
#define B134 134
#define B150 150
#define B200 200
#define B300 300
#define B600 600
#define B1200 1200
#define B1800 1800
#define B2400 2400
#define B4800 4800
#define B9600 9600
char *strtok(char *, const char *);
int execlp(const char *, const char *, ...);
