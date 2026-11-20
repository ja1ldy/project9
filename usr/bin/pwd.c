/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(void)
{
  char path[512];

  if(getcwd(path, sizeof(path)) >= 0)
    printf(1, "%s\n", path);
  else
    printf(2, "pwd: cannot determine current directory\n");
  exit();
}
