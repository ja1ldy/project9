/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  uint first = 1, last;

  if(argc < 2 || argc > 3 ||
     (argc == 3 && parseuint(argv[1], &first) < 0) ||
     parseuint(argv[argc-1], &last) < 0){
    printf(2, "usage: seq [first] last (nonnegative integers)\n");
    exit();
  }
  while(first <= last){
    printf(1, "%d\n", first++);
  }
  exit();
}
