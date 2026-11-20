/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "text.h"

static uchar selected[TEXT_LINE];
static char line[TEXT_LINE], output[TEXT_LINE];

static int
number(const char **cursor)
{
  const char *p = *cursor;
  int n = 0;

  while(*p >= '0' && *p <= '9'){
    n = n*10 + *p++ - '0';
    if(n >= TEXT_LINE)
      return -1;
  }
  *cursor = p;
  return n;
}

static int
select_columns(const char *p)
{
  int lo, hi;

  if(!*p)
    return -1;
  for(;;){
    lo = number(&p);
    if(lo < 0)
      return -1;
    hi = lo;
    if(*p == '-'){
      p++;
      hi = number(&p);
      if(hi < 0 || (!lo && !hi))
        return -1;
      if(!lo)
        lo = 1;
      if(!hi)
        hi = TEXT_LINE - 1;
    }
    if(!lo || lo > hi)
      return -1;
    for(; lo <= hi; lo++)
      selected[lo] = 1;
    if(!*p)
      return 0;
    if(*p++ != ',')
      return -1;
  }
}

static int
cut(int fd, int fields, char delimiter, int suppress)
{
  struct text_input in = {.fd = fd};
  char *p, *start;
  int status, len, column, any;

  while((status = textline(&in, line, sizeof(line))) > 0){
    len = 0;
    if(!fields){
      for(p = line, column = 1; *p; p++, column++)
        if(selected[column])
          output[len++] = *p;
    } else if(!strchr(line, delimiter)){
      if(suppress)
        continue;
      len = strlen(line);
      memmove(output, line, len);
    } else {
      start = line;
      any = 0;
      for(p = line, column = 1; ; p++){
        if(*p && *p != delimiter)
          continue;
        if(column < TEXT_LINE && selected[column]){
          if(any)
            output[len++] = delimiter;
          memmove(output+len, start, p-start);
          len += p-start;
          any = 1;
        }
        if(!*p)
          break;
        column++;
        start = p+1;
      }
    }
    output[len++] = '\n';
    if(write(1, output, len) != len)
      return 1;
  }
  if(status < 0)
    printf(2, "cut: input error or line too long\n");
  return status < 0;
}

int
main(int argc, char **argv)
{
  char delimiter = '\t';
  const char *list = 0;
  int mode = 0, suppress = 0, i = 1, fd, failed = 0;

  while(i < argc){
    if(!strcmp(argv[i], "-c") || !strcmp(argv[i], "-f")){
      if(mode || i + 1 == argc)
        goto usage;
      mode = argv[i][1];
      list = argv[i+1];
      i += 2;
    } else if(!strcmp(argv[i], "-d")){
      if(i + 1 == argc || strlen(argv[i+1]) != 1)
        goto usage;
      delimiter = argv[i+1][0];
      i += 2;
    } else if(!strcmp(argv[i], "-s")){
      suppress = 1;
      i++;
    } else {
      if(!strcmp(argv[i], "--"))
        i++;
      break;
    }
  }
  if(!mode || (mode != 'f' && (delimiter != '\t' || suppress)) || select_columns(list) < 0)
    goto usage;
  if(i == argc)
    return cut(0, mode == 'f', delimiter, suppress);
  for(; i < argc; i++){
    if(!strcmp(argv[i], "-")){
      failed |= cut(0, mode == 'f', delimiter, suppress);
    } else if((fd = open(argv[i], O_RDONLY)) < 0){
      printf(2, "cut: cannot open %s\n", argv[i]);
      failed = 1;
    } else {
      failed |= cut(fd, mode == 'f', delimiter, suppress);
      close(fd);
    }
  }
  return failed;

usage:
  printf(2, "usage: cut -c list [file ...] | cut [-s] [-d delimiter] -f list [file ...]\n");
  return 2;
}
