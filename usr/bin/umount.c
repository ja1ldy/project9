/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "diskuser.h"
#include "p9abi.h"

int
main(int argc, char **argv)
{
  if(argc != 2 || strcmp(argv[1], "/mnt")){
    printf(2, "usage: umount /mnt\n");
    return 2;
  }
  int result = installmount(0, 1);
  if(result < 0){
    printf(2, "umount: %s\n", result == -P9_EIO ?
           "disk I/O failed while synchronizing" : result == -P9_EBUSY ?
           "/mnt is busy" : "requires a mounted installer target and root");
    return 1;
  }
  return 0;
}
