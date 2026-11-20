/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"

char *__progname = "top";

void
warnx(const char *fmt, ...)
{
  va_list args;
  fputs("top: ", stderr);
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputc('\n', stderr);
}

void
err(int status, const char *fmt, ...)
{
  int error = errno;
  va_list args;
  fputs("top: ", stderr);
  if(fmt){
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputs(": ", stderr);
  }
  fprintf(stderr, "%s\n", strerror(error));
  exit(status);
}

long long
strtonum(const char *s, long long low, long long high, const char **error)
{
  char *end;
  errno = 0;
  long value = strtol(s, &end, 10);
  *error = end == s || *end ? "invalid" :
    value < low || (errno == ERANGE && value == LONG_MIN) ? "too small" :
    value > high || errno == ERANGE ? "too large" : NULL;
  return *error ? 0 : value;
}

size_t
strlcat(char *out, const char *src, size_t size)
{
  size_t have = 0;
  while(have < size && out[have])
    have++;
  if(have == size)
    return have + strlen(src);
  return have + strlcpy(out + have, src, size - have);
}

int
strncasecmp(const char *a, const char *b, size_t n)
{
  for(size_t i = 0; i < n; ++i){
    unsigned char x = a[i], y = b[i];
    int cmp = tolower(x) - tolower(y);
    if(cmp || !x)
      return cmp;
  }
  return 0;
}

int
strcasecmp(const char *a, const char *b)
{
  return strncasecmp(a, b, (size_t)-1);
}
