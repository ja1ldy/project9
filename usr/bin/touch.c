/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  int i, fd;

  if(argc < 2){
    printf(2, "Usage: touch file...\n");
    exit();
  }

  for(i = 1; i < argc; i++){
    fd = open(argv[i], O_CREATE | O_RDWR);
    if(fd < 0){
      printf(2, "touch: cannot create %s\n", argv[i]);
      continue;
    }
    close(fd);
  }
  exit();
}
