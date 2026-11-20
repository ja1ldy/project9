/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "param.h"

int
main(int argc, char **argv)
{
  int i, n, first = 1, mode = O_WRONLY | O_CREATE | O_TRUNC;
  int fd[NOFILE], count = 0;
  char buf[512];

  if(argc > 1 && strcmp(argv[1], "-a") == 0){
    first++;
    mode = O_WRONLY | O_CREATE | O_APPEND;
  }
  if(argc - first > NOFILE - 3){
    printf(2, "tee: too many output files\n");
    exit();
  }
  for(i = first; i < argc; i++){
    fd[count] = open(argv[i], mode);
    if(fd[count] < 0)
      printf(2, "tee: cannot open %s\n", argv[i]);
    count++;
  }
  while((n = read(0, buf, sizeof(buf))) > 0){
    if(write(1, buf, n) != n)
      break;
    for(i = 0; i < count; i++){
      if(fd[i] >= 0 && write(fd[i], buf, n) != n){
        printf(2, "tee: cannot write %s\n", argv[first+i]);
        close(fd[i]);
        fd[i] = -1;
      }
    }
  }
  if(n < 0)
    printf(2, "tee: read failed\n");
  exit();
}
