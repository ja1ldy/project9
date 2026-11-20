/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"

char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

void*
memset(void *dst, int c, uint n)
{
  stosb(dst, c, n);
  return dst;
}

char*
strchr(const char *s, int c)
{
  do {
    if((uchar)*s == (uchar)c)
      return (char*)s;
  } while(*s++);
  return 0;
}

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

void*
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  if(n > 0 && (uptr)dst > (uptr)src && (uptr)dst - (uptr)src < (uptr)n){
    while(n > 0){
      n--;
      dst[n] = src[n];
    }
  } else {
    while(n-- > 0)
      *dst++ = *src++;
  }
  return vdst;
}

int
parseuint(const char *s, uint *out)
{
  uint value = 0, digit;

  if(!*s)
    return -1;
  while(*s){
    digit = *s++ - '0';
    if(digit > 9 || value > (2147483647U - digit) / 10)
      return -1;
    value = value * 10 + digit;
  }
  *out = value;
  return 0;
}

int
parsemode(const char *s, uint *out)
{
  uint mode = 0;
  int digits = 0;

  while(*s >= '0' && *s <= '7' && digits < 4){
    mode = mode * 8 + *s++ - '0';
    digits++;
  }
  if(!digits || *s)
    return -1;
  *out = mode;
  return 0;
}

void
formatmode(uint mode, char text[5])
{
  int i;

  text[4] = 0;
  for(i = 3; i >= 0; i--){
    text[i] = '0' + (mode & 7);
    mode >>= 3;
  }
}

int
parseint(const char *text, int *out)
{
  int negative = *text == '-';
  uint value = 0, limit = 2147483647U + negative, digit;

  if(*text == '-' || *text == '+')
    text++;
  if(!*text)
    return -1;
  for(; *text; text++){
    digit = *text - '0';
    if(digit > 9 || value > (limit - digit) / 10)
      return -1;
    value = value * 10 + digit;
  }
  *out = negative ? 0U - value : value;
  return 0;
}
