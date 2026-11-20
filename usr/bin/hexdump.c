/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fcntl.h"

static const char hex[] = "0123456789abcdef";

static char *
offset(char *p, uint n)
{
  int shift;

  for(shift = 28; shift >= 0; shift -= 4)
    *p++ = hex[(n >> shift) & 15];
  return p;
}

int
main(int argc, char **argv)
{
  uchar data[16];
  uint addr = 0;
  int fd = 0, n, used, i;
  char line[80], *p;

  if(argc > 1 && !strcmp(argv[1], "-C")){
    argc--;
    argv++;
  }
  if(argc > 2){
    printf(2, "usage: hexdump [-C] [file]\n");
    return 2;
  }
  if(argc == 2 && strcmp(argv[1], "-") && (fd = open(argv[1], O_RDONLY)) < 0){
    printf(2, "hexdump: cannot open %s\n", argv[1]);
    return 1;
  }
  for(;;){
    used = 0;
    while(used < sizeof(data) && (n = read(fd, data+used, sizeof(data)-used)) > 0)
      used += n;
    if(n < 0){
      printf(2, "hexdump: read error\n");
      return 1;
    }
    if(!used)
      break;
    p = offset(line, addr);
    *p++ = ' ';
    for(i = 0; i < sizeof(data); i++){
      if(i == 8)
        *p++ = ' ';
      *p++ = ' ';
      *p++ = i < used ? hex[data[i] >> 4] : ' ';
      *p++ = i < used ? hex[data[i] & 15] : ' ';
    }
    *p++ = ' ';
    *p++ = ' ';
    *p++ = '|';
    for(i = 0; i < used; i++)
      *p++ = data[i] >= 32 && data[i] < 127 ? data[i] : '.';
    *p++ = '|';
    *p++ = '\n';
    if(write(1, line, p-line) != p-line)
      return 1;
    addr += used;
  }
  p = offset(line, addr);
  *p++ = '\n';
  return write(1, line, p-line) == p-line ? 0 : 1;
}
