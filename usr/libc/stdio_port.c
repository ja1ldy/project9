/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <project9/param.h>

int
fileno(FILE *f)
{
  if(f->_file < 0)
    errno = EBADF;
  return f->_file;
}

void
perror(const char *s)
{
  int err = errno;

  if(s && *s)
    fprintf(stderr, "%s: ", s);
  fprintf(stderr, "%s\n", strerror(err));
  errno = err;
}

long
sysconf(int name)
{
  switch(name){
  case _SC_OPEN_MAX: return NOFILE;
  case _SC_PAGESIZE: return getpagesize();
  default: errno = EINVAL; return -1;
  }
}

void
__p9_assert(const char *condition, const char *file, int line)
{
  fprintf(stderr, "assertion failed: %s at %s:%d\n", condition, file, line);
  abort();
}
