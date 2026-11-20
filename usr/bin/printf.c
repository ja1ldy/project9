/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

static int
digit(char c)
{
  if(c >= '0' && c <= '9')
    return c - '0';
  if(c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if(c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static char
escaped(const char **next)
{
  const char *p = *next;
  int base = 8, left = 3, d;
  unsigned int value = 0;
  char c;

  switch(*p){
  case 'a': c = '\a'; break;
  case 'b': c = '\b'; break;
  case 'f': c = '\f'; break;
  case 'n': c = '\n'; break;
  case 'r': c = '\r'; break;
  case 't': c = '\t'; break;
  case 'v': c = '\v'; break;
  case '\\': c = '\\'; break;
  default:
    if(*p == 'x'){
      p++;
      base = 16;
      left = 2;
    }
    d = digit(*p);
    if(d < 0 || d >= base)
      return '\\';
    do {
      value = value * base + d;
      p++;
      d = digit(*p);
    } while(--left && d >= 0 && d < base);
    *next = p;
    return (uchar)value;
  }
  *next = p + 1;
  return c;
}

int
main(int argc, char **argv)
{
  const char *format, *p, *arg;
  char c;
  int i = 2, used, n;

  if(argc < 2){
    printf(2, "usage: printf format [argument ...]\n");
    return 2;
  }
  format = argv[1];
  do {
    used = i;
    for(p = format; (c = *p++) != 0; ){
      if(c == '\\' && *p){
        c = escaped(&p);
      } else if(c == '%'){
        c = *p++;
        if(c != '%'){
          arg = i < argc ? argv[i++] : "";
          switch(c){
          case 's': printf(1, "%s", arg); continue;
          case 'c': if(!*arg) continue; c = *arg; break;
          case 'd':
            n = 0;
            if(*arg && parseint(arg, &n) < 0){
              printf(2, "printf: expected an integer\n");
              return 1;
            }
            printf(1, "%d", n);
            continue;
          default:
            printf(2, "printf: invalid format\n");
            return 2;
          }
        }
      }
      if(write(1, &c, 1) != 1)
        return 1;
    }
  } while(i < argc && i != used);
  return 0;
}
