/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  int i = 1, failed = 0;

  if(i < argc && !strcmp(argv[i], "--"))
    i++;
  if(i == argc){
    printf(2, "usage: rm file ...\n");
    return 2;
  }
  for(; i < argc; i++)
    if(unlink(argv[i]) < 0){
      printf(2, "rm: %s failed to delete\n", argv[i]);
      failed = 1;
    }
  return failed;
}
