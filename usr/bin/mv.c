/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  if(argc != 3){
    printf(2, "usage: mv old new\n");
    return 2;
  }
  if(rename(argv[1], argv[2]) < 0){
    printf(2, "mv: cannot move %s to %s\n", argv[1], argv[2]);
    return 1;
  }
  return 0;
}
