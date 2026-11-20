/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void *raw_malloc(size_t) __asm__("malloc");
extern size_t p9_allocsize(void *);

void *
malloc(size_t n)
{
  void *p = raw_malloc(n);
  if(!p)
    errno = ENOMEM;
  return p;
}

void *
calloc(size_t n, size_t size)
{
  void *p;

  if(size && n > SIZE_MAX / size){
    errno = ENOMEM;
    return NULL;
  }
  size *= n;
  p = malloc(size);
  return p ? memset(p, 0, size) : NULL;
}

void *
realloc(void *old, size_t n)
{
  size_t capacity;
  void *p;

  if(!old)
    return malloc(n);
  if(!n){
    free(old);
    return NULL;
  }
  capacity = p9_allocsize(old);
  if(capacity >= n)
    return old;
  p = malloc(n);
  if(p){
    memcpy(p, old, capacity);
    free(old);
  }
  return p;
}

int isdigit(int c) { return c >= '0' && c <= '9'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int isalpha(int c) { return islower(c) || isupper(c); }
int isblank(int c) { return c == ' ' || c == '\t'; }
int iscntrl(int c) { return (c >= 0 && c < ' ') || c == 127; }
int isgraph(int c) { return c > ' ' && c <= '~'; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
int isspace_l(int c, locale_t loc) { (void)loc; return isspace(c); }
int isprint(int c) { return c >= ' ' && c <= '~'; }
int isxdigit(int c) { return isdigit(c) || (tolower(c) >= 'a' && tolower(c) <= 'f'); }
int tolower(int c) { return isupper(c) ? c + 'a' - 'A' : c; }
int toupper(int c) { return islower(c) ? c + 'A' - 'a' : c; }

int atoi(const char *s) { return strtol(s, NULL, 10); }
int abs(int n) { return n < 0 ? 0U - (unsigned)n : n; }

void (*__cleanup)(void);
static void (*exits[32])(void);
static int nexits;

int
atexit(void (*handler)(void))
{
  if(!handler || nexits == sizeof(exits)/sizeof(*exits)){
    errno = ENOMEM;
    return -1;
  }
  exits[nexits++] = handler;
  return 0;
}

void
exit(int status)
{
  while(nexits)
    exits[--nexits]();
  if(__cleanup)
    __cleanup();
  _exit(status);
}

char *
strerror(int error)
{
  switch(error){
  case 0: return "No error";
  case EPERM: return "Operation not permitted";
  case ENOENT: return "No such file";
  case EINTR: return "Interrupted system call";
  case EIO: return "Input/output error";
  case EBADF: return "Bad file descriptor";
  case EAGAIN: return "Resource temporarily unavailable";
  case ENOMEM: return "Out of memory";
  case EACCES: return "Permission denied";
  case EFAULT: return "Bad address";
  case EBUSY: return "Resource busy";
  case EEXIST: return "File exists";
  case ENODEV: return "No such device";
  case ENOTDIR: return "Not a directory";
  case EINVAL: return "Invalid argument";
  case EMFILE: return "Too many open files";
  case ENOTTY: return "Inappropriate ioctl";
  case ESPIPE: return "Illegal seek";
  case EOVERFLOW: return "Value too large";
  case ENOSPC: return "No space left on device";
  case EPIPE: return "Broken pipe";
  case ERANGE: return "Result out of range";
  case ENOSYS: return "Function not supported";
  default: return "Unknown error";
  }
}

void *
reallocarray(void *p, size_t count, size_t size)
{
  if(size && count > SIZE_MAX / size){
    errno = ENOMEM;
    return NULL;
  }
  return realloc(p, count * size);
}

void
abort(void)
{
  _exit(134);
}
