/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "diskuser.h"
#include "p9abi.h"

int
main(void)
{
  int result = disksync();
  if(result < 0){
    printf(2, "sync: %s\n", result == -P9_EIO ?
           "disk I/O failed while synchronizing" : "filesystem is busy");
    return 1;
  }
  return 0;
}
