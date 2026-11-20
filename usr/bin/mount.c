/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "diskuser.h"

int
main(int argc, char **argv)
{
  int id;

  if(argc != 3 || (id = diskid(argv[1], 1)) < 0 || strcmp(argv[2], "/mnt")){
    printf(2, "usage: mount partition /mnt\n");
    return 2;
  }
  if(installmount(id, 0) < 0){
    printf(2, "mount: requires an unused /mnt and a clean test partition in the live session\n");
    return 1;
  }
  return 0;
}
