/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

static char buf[512];

static int
cat(int fd)
{
  int n;

  while((n = read(fd, buf, sizeof(buf))) > 0){
    if(write(1, buf, n) != n){
      printf(2, "cat: write error\n");
      return 1;
    }
  }
  if(n < 0)
    printf(2, "cat: read error\n");
  return n < 0;
}

int
main(int argc, char **argv)
{
  int fd, i, status = 0;

  if(argc == 1)
    return cat(0);
  for(i = 1; i < argc; i++){
    if(!strcmp(argv[i], "-")){
      status |= cat(0);
      continue;
    }
    if((fd = open(argv[i], 0)) < 0){
      printf(2, "cat: cannot open %s\n", argv[i]);
      status = 1;
    } else {
      status |= cat(fd);
      close(fd);
    }
  }
  return status;
}
