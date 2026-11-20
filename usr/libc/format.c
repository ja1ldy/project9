/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct output { char *buf; size_t size; FILE *file; int n, failed; };
enum { LEFT = 1, ZERO = 2, PLUS = 4, SPACE = 8, ALT = 16 };

static void
put(struct output *out, int c)
{
  if(out->failed)
    return;
  if(out->n == INT_MAX){
    errno = ERANGE;
    out->failed = 1;
    return;
  }
  if(out->file){
    if(fputc(c, out->file) == EOF)
      out->failed = 1;
  } else if(out->size && (size_t)out->n < out->size-1)
    out->buf[out->n] = c;
  out->n++;
}

static void
pad(struct output *out, int c, int n)
{
  if(!out->file && !out->failed && n > 0){
    size_t end = out->size ? out->size-1 : 0;
    size_t now = out->n, room = end > now ? end-now : 0;
    if(n > INT_MAX - out->n){
      out->failed = 1;
      errno = ERANGE;
      return;
    }
    if(room)
      memset(out->buf+now, c, room < (size_t)n ? room : (size_t)n);
    out->n += n;
    return;
  }
  while(n-- > 0 && !out->failed)
    put(out, c);
}

static int
number(const char **text)
{
  int n = 0, digit;
  while(isdigit(**text)){
    digit = *(*text)++ - '0';
    n = n > (INT_MAX-digit) / 10 ? INT_MAX : n*10+digit;
  }
  return n;
}

static void
field(struct output *out, const char *s, int n, int width, int flags)
{
  if(!(flags & LEFT))
    pad(out, ' ', width-n);
  for(int i = 0; i < n; i++)
    put(out, s[i]);
  if(flags & LEFT)
    pad(out, ' ', width-n);
}

static void
numeric(struct output *out, unsigned long n, int sign, int base, int upper,
        int precision, int width, int flags, int pointer)
{
  const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  char tmp[32], prefix[3];
  int used = 0, p = 0, zeros, spaces, i;
  unsigned long original = n;

  if(n || precision)
    do {
      tmp[used++] = digits[n%base];
      n /= base;
    } while(n);
  if(sign)
    prefix[p++] = sign;
  if(pointer || ((flags & ALT) && base == 16 && original)){
    prefix[p++] = '0';
    prefix[p++] = upper ? 'X' : 'x';
  }
  if((flags & ALT) && base == 8 && (!used || original) && precision <= used)
    precision = used + 1;
  zeros = precision > used ? precision-used : 0;
  spaces = width > p+used ? width-p-used : 0;
  spaces = spaces > zeros ? spaces-zeros : 0;
  if((flags & ZERO) && !(flags & LEFT) && precision < 0){
    zeros = spaces;
    spaces = 0;
  }
  if(!(flags & LEFT))
    pad(out, ' ', spaces);
  for(i = 0; i < p; i++)
    put(out, prefix[i]);
  pad(out, '0', zeros);
  while(used)
    put(out, tmp[--used]);
  if(flags & LEFT)
    pad(out, ' ', spaces);
}

extern char *__dtoa(double, int, int, int *, int *, char **);
extern void __freedtoa(char *);

static void
floatdigits(struct output *out, const char *digits, int count, int start, int n)
{
  int zeros;

  if(start < 0){
    zeros = -start < n ? -start : n;
    pad(out, '0', zeros);
    start += zeros;
    n -= zeros;
  }
  for(; n && start < count; n--, start++)
    put(out, digits[start]);
  pad(out, '0', n);
}

static void
floating(struct output *out, double x, int conversion, int precision,
         int width, int flags)
{
  int upper = isupper(conversion), kind = tolower(conversion);
  int point, negative, exponent, n, request, scientific, frac, whole;
  int sign, spaces, decimal, expdigits = 0, expvalue = 0, i;
  char *digits, *end, expbuf[4];
  const char *special = NULL;
  long long body;

  if(precision < 0)
    precision = 6;
  if(kind == 'g' && !precision)
    precision = 1;
  request = precision > 1074 ? 1074 : precision;
  digits = __dtoa(x, kind == 'f' ? 3 : 2, request + (kind == 'e'),
                  &point, &negative, &end);
  if(!digits){
    errno = ENOMEM;
    out->failed = 1;
    return;
  }
  n = end - digits;
  sign = negative ? '-' : flags & PLUS ? '+' : flags & SPACE ? ' ' : 0;
  if(point == 9999){
    int inf = tolower(digits[0]) == 'i';
    special = inf ? (upper ? "INF" : "inf") : (upper ? "NAN" : "nan");
    spaces = width > 3 + !!sign ? width - 3 - !!sign : 0;
    if(!(flags & LEFT))
      pad(out, ' ', spaces);
    if(sign)
      put(out, sign);
    for(i = 0; i < 3; i++)
      put(out, special[i]);
    if(flags & LEFT)
      pad(out, ' ', spaces);
    __freedtoa(digits);
    return;
  }

  exponent = point - 1;
  scientific = kind == 'e' || (kind == 'g' && (exponent < -4 || exponent >= precision));
  whole = scientific || point < 1 ? 1 : point;
  if(kind == 'g'){
    long long wanted = (long long)(flags & ALT ? precision : n) -
                       (scientific ? 1 : point);
    if(wanted > INT_MAX)
      goto overflow;
    frac = wanted > 0 ? wanted : 0;
  } else
    frac = precision;
  decimal = frac || (flags & ALT);
  if(scientific){
    expvalue = exponent < 0 ? -exponent : exponent;
    do {
      expbuf[expdigits++] = '0' + expvalue%10;
      expvalue /= 10;
    } while(expvalue || expdigits < 2);
  }
  body = (long long)whole + decimal + frac + !!sign + (scientific ? 2+expdigits : 0);
  if(body > INT_MAX)
    goto overflow;
  spaces = width > body ? width - (int)body : 0;
  if(!(flags & (LEFT | ZERO)))
    pad(out, ' ', spaces);
  if(sign)
    put(out, sign);
  if((flags & ZERO) && !(flags & LEFT))
    pad(out, '0', spaces);
  if(!scientific && point <= 0)
    put(out, '0');
  else
    floatdigits(out, digits, n, 0, whole);
  if(decimal)
    put(out, '.');
  floatdigits(out, digits, n, scientific ? 1 : point, frac);
  if(scientific){
    put(out, upper ? 'E' : 'e');
    put(out, exponent < 0 ? '-' : '+');
    while(expdigits)
      put(out, expbuf[--expdigits]);
  }
  if(flags & LEFT)
    pad(out, ' ', spaces);
  __freedtoa(digits);
  return;

overflow:
  __freedtoa(digits);
  errno = ERANGE;
  out->failed = 1;
}

static int
format(struct output *out, const char *fmt, va_list ap)
{
  int c, flags, width, precision, length, n, base, sign;
  unsigned long value;
  long signedval;
  const char *s;
  char ch;

  while(*fmt && !out->failed){
    c = *fmt++;
    if(c != '%'){
      put(out, c);
      continue;
    }
    flags = 0;
    for(;;){
      if(*fmt == '-') flags |= LEFT;
      else if(*fmt == '0') flags |= ZERO;
      else if(*fmt == '+') flags |= PLUS;
      else if(*fmt == ' ') flags |= SPACE;
      else if(*fmt == '#') flags |= ALT;
      else break;
      fmt++;
    }
    if(*fmt == '*'){
      fmt++;
      width = va_arg(ap, int);
      if(width < 0){
        flags |= LEFT;
        width = width == INT_MIN ? INT_MAX : -width;
      }
    } else
      width = number(&fmt);
    precision = -1;
    if(*fmt == '.'){
      fmt++;
      if(*fmt == '*'){
        fmt++;
        precision = va_arg(ap, int);
        if(precision < 0)
          precision = -1;
      } else
        precision = number(&fmt);
    }
    length = *fmt == 'l' ? 1 : *fmt == 'z' ? 2 : 0;
    if(length)
      fmt++;
    c = *fmt++;
    if(c == '%')
      put(out, '%');
    else if(c == 's' || c == 'm'){
      s = c == 's' ? va_arg(ap, const char *) : strerror(errno);
      if(!s)
        s = "(null)";
      for(n = 0; (precision < 0 || n < precision) && s[n]; n++)
        ;
      field(out, s, n, width, flags);
    } else if(c == 'c'){
      ch = va_arg(ap, int);
      field(out, &ch, 1, width, flags);
    } else if(c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'g' || c == 'G'){
      floating(out, va_arg(ap, double), c, precision, width, flags);
    } else if(c == 'd' || c == 'i' || c == 'u' || c == 'o' ||
              c == 'x' || c == 'X' || c == 'p'){
      sign = 0;
      base = c == 'x' || c == 'X' || c == 'p' ? 16 : c == 'o' ? 8 : 10;
      if(c == 'd' || c == 'i'){
        signedval = length == 1 ? va_arg(ap, long) : va_arg(ap, int);
        sign = signedval < 0 ? '-' : flags & PLUS ? '+' : flags & SPACE ? ' ' : 0;
        value = signedval < 0 ? 0UL - (unsigned long)signedval : (unsigned long)signedval;
      } else if(c == 'p')
        value = (uintptr_t)va_arg(ap, void *);
      else
        value = length == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
      numeric(out, value, sign, base, c == 'X', precision, width, flags, c == 'p');
    } else {
      errno = EINVAL;
      out->failed = 1;
    }
  }
  return out->failed ? -1 : out->n;
}

int
vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
  struct output out = { .buf = buf, .size = size };
  int n = format(&out, fmt, ap);
  if(size)
    buf[(size_t)out.n < size ? (size_t)out.n : size-1] = 0;
  return n;
}

int
vfprintf(FILE *f, const char *fmt, va_list ap)
{
  struct output out = { .file = f };
  return format(&out, fmt, ap);
}

int vsprintf(char *buf, const char *fmt, va_list ap) { return vsnprintf(buf, SIZE_MAX, fmt, ap); }

#define FORMATTED(name, params, call) \
  int name params { va_list ap; int n; va_start(ap, fmt); n = call; va_end(ap); return n; }
FORMATTED(printf, (const char *fmt, ...), vfprintf(stdout, fmt, ap))
FORMATTED(fprintf, (FILE *f, const char *fmt, ...), vfprintf(f, fmt, ap))
FORMATTED(sprintf, (char *b, const char *fmt, ...), vsprintf(b, fmt, ap))
FORMATTED(snprintf, (char *b, size_t len, const char *fmt, ...), vsnprintf(b, len, fmt, ap))

static int
hex(int c)
{
  if(isdigit(c))
    return c - '0';
  return isxdigit(c) ? tolower(c)-'a'+10 : -1;
}

int
sscanf(const char *s, const char *f, ...)
{
  va_list ap;
  int matched = 0, converted = 0, width, c, suppress, length, n, base, negative, d;
  unsigned long value;
  const char *start;
  char *dest;

  va_start(ap, f);
  while(*f){
    c = *f++;
    if(isspace(c)){
      while(isspace(*s))
        s++;
      continue;
    }
    if(c != '%' || *f == '%'){
      if(c == '%')
        f++;
      if(!*s){
        if(!converted) matched = EOF;
        break;
      }
      if(*s++ != c)
        break;
      continue;
    }
    suppress = *f == '*';
    f += suppress;
    width = number(&f);
    length = *f == 'l' || *f == 'z';
    f += length;
    c = *f++;
    if(c != 'c')
      while(isspace(*s))
        s++;
    if(!*s){
      if(!converted) matched = EOF;
      break;
    }
    if(c == 's' || c == 'c'){
      if(!width)
        width = c == 'c' ? 1 : INT_MAX;
      for(n = 0; n < width && s[n] && (c == 'c' || !isspace(s[n])); n++)
        ;
      if(c == 'c' && n != width){
        if(!converted) matched = EOF;
        break;
      }
      if(!suppress){
        dest = va_arg(ap, char *);
        memcpy(dest, s, n);
        if(c == 's')
          dest[n] = 0;
      }
      s += n;
    } else if(c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'X' || c == 'o'){
      if(!width)
        width = INT_MAX;
      base = c == 'x' || c == 'X' ? 16 : c == 'o' ? 8 : 10;
      negative = *s == '-';
      if(*s == '-' || *s == '+'){
        s++;
        width--;
      }
      if(c == 'i' && width > 0 && *s == '0')
        base = 8;
      if((base == 16 || c == 'i') && width > 2 &&
         s[0] == '0' && tolower(s[1]) == 'x' && isxdigit(s[2])){
        s += 2;
        width -= 2;
        base = 16;
      }
      start = s;
      value = 0;
      while(width-- > 0 && (d = hex(*s)) >= 0 && d < base){
        value = value * base + d;
        s++;
      }
      if(s == start)
        break;
      if(negative)
        value = 0UL-value;
      if(!suppress){
        if(c == 'd' || c == 'i'){
          if(length) *va_arg(ap, long *) = value;
          else *va_arg(ap, int *) = value;
        } else {
          if(length) *va_arg(ap, unsigned long *) = value;
          else *va_arg(ap, unsigned int *) = value;
        }
      }
    } else
      break;
    converted++;
    matched += !suppress;
  }
  va_end(ap);
  return matched;
}
