/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  uint mode;
  int i, failed = 0;

  if(argc < 3 || parsemode(argv[1], &mode) < 0){
    printf(2, "usage: chmod octal-mode file ...\n");
    exitstatus(1);
  }
  for(i = 2; i < argc; i++){
    if(chmod(argv[i], mode) < 0){
      printf(2, "chmod: %s: cannot change mode\n", argv[i]);
      failed = 1;
    }
  }
  exitstatus(failed);
}
