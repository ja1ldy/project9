/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

struct output {
  int fd, size, failed;
  char buf[128];
};

static void
flush(struct output *out)
{
  if(out->size && write(out->fd, out->buf, out->size) != out->size)
    out->failed = 1;
  out->size = 0;
}

static void
putc(struct output *out, char c)
{
  out->buf[out->size++] = c;
  if(out->size == sizeof(out->buf))
    flush(out);
}

static void
printint(struct output *out, uptr number, int base, int neg)
{
  const char *digits = "0123456789ABCDEF";
  char buf[2 * sizeof(uptr) + 1];
  int len = 0;

  do {
    buf[len++] = digits[number % base];
    number /= base;
  } while(number);
  if(neg)
    buf[len++] = '-';
  while(len)
    putc(out, buf[--len]);
}

void
printf(int fd, const char *fmt, ...)
{
  struct output out = {.fd = fd};
  __builtin_va_list ap;
  const char *s;
  int c, n;

  __builtin_va_start(ap, fmt);
  while(!out.failed && (c = *fmt++) != 0){
    if(c != '%'){
      putc(&out, c);
      continue;
    }

    c = *fmt++;
    if(!c)
      break;
    switch(c){
    case 'd':
      n = __builtin_va_arg(ap, int);
      printint(&out, n < 0 ? 0U - n : (uint)n, 10, n < 0);
      break;
    case 'u':
      printint(&out, __builtin_va_arg(ap, uint), 10, 0);
      break;
    case 'x':
      printint(&out, __builtin_va_arg(ap, uint), 16, 0);
      break;
    case 'p':
      printint(&out, (uptr)__builtin_va_arg(ap, void*), 16, 0);
      break;
    case 's':
      s = __builtin_va_arg(ap, const char*);
      if(!s)
        s = "(null)";
      while(!out.failed && *s)
        putc(&out, *s++);
      break;
    case 'c':
      putc(&out, __builtin_va_arg(ap, int));
      break;
    default:
      if(c != '%')
        putc(&out, '%');
      putc(&out, c);
      break;
    }
  }
  __builtin_va_end(ap);
  if(!out.failed)
    flush(&out);
}
