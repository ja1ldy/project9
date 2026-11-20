/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  int i;

  if(argc < 2)
    printf(2, "usage: rmdir dir ...\n");
  for(i = 1; i < argc; i++)
    if(unlinkdir(argv[i]) < 0)
      printf(2, "rmdir: cannot remove %s\n", argv[i]);
  exit();
}
