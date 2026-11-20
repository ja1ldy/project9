/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "reboot.h"
#include "p9abi.h"

int
main(int argc, char **argv)
{
  if(argc != 1){
    printf(2, "usage: reboot\n");
    return 2;
  }
  int error = reboot(RB_REBOOT);
  printf(2, "reboot: %s\n", error == -P9_EIO ?
         "disk I/O failed while synchronizing" : error == -P9_EBUSY ?
         "filesystem is busy" : "root permission required");
  return 1;
}
