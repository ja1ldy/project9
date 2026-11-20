/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "user.h"

#define MAXBUF 8192
char data[MAXBUF];

void
tail(int fd, int lines)
{
  int n, total = 0, i, count = 0;

  while((n = read(fd, data + total, MAXBUF - total - 1)) > 0){
    total += n;
    if(total >= MAXBUF - 1)
      break;
  }
  data[total] = 0;

  for(i = total - 1; i >= 0; i--){
    if(data[i] == '\n'){
      if(i != total - 1)
        count++;
      if(count == lines){
        i++;
        break;
      }
    }
  }
  if(i < 0)
    i = 0;

  write(1, data + i, total - i);
}

int
main(int argc, char *argv[])
{
  int fd, i, lines = 10, arg_idx = 1;

  if(argc > 1 && argv[1][0] == '-'){
    lines = atoi(argv[1] + 1);
    arg_idx = 2;
  }

  if(argc <= arg_idx){
    tail(0, lines);
    exit();
  }

  for(i = arg_idx; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      printf(2, "tail: cannot open %s\n", argv[i]);
      exit();
    }
    tail(fd, lines);
    close(fd);
  }
  exit();
}
