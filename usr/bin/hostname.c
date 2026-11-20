/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fcntl.h"

#define BUFSZ 64

int
main(int argc, char *argv[])
{
  int fd, n;
  char buf[BUFSZ];

  if(argc > 1){
    if(getuid() != 0){
      printf(2, "hostname: permission denied (root required)\n");
      exit();
    }
    if((fd = open("/etc/hostname", O_CREATE | O_WRONLY | O_TRUNC)) < 0){
      printf(2, "hostname: cannot write /etc/hostname\n");
      exit();
    }
    if(write(fd, argv[1], strlen(argv[1])) != strlen(argv[1]) ||
       write(fd, "\n", 1) != 1)
      printf(2, "hostname: write failed\n");
    close(fd);
    exit();
  }

  if((fd = open("/etc/hostname", O_RDONLY)) < 0){
    printf(1, "xv6\n");
    exit();
  }
  while((n = read(fd, buf, sizeof(buf))) > 0)
    write(1, buf, n);
  close(fd);
  exit();
}
