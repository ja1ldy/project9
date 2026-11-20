/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "user.h"

char buf[512];

void
head(int fd, int lines)
{
  int n, i, l = 0;

  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(i = 0; i < n; i++){
      printf(1, "%c", buf[i]);
      if(buf[i] == '\n'){
        l++;
        if(l >= lines)
          return;
      }
    }
  }
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
    head(0, lines);
    exit();
  }

  for(i = arg_idx; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      printf(2, "head: cannot open %s\n", argv[i]);
      exit();
    }
    head(fd, lines);
    close(fd);
  }
  exit();
}
