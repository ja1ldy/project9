/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  char *base, *end;
  uint suffix;

  if(argc < 2 || argc > 3){
    printf(2, "usage: basename path [suffix]\n");
    exit();
  }
  base = argv[1];
  end = base + strlen(base);
  while(end > base+1 && end[-1] == '/')
    *--end = 0;
  if(strcmp(base, "/") != 0){
    for(base = end; base > argv[1] && base[-1] != '/'; base--)
      ;
  }
  if(argc == 3){
    suffix = strlen(argv[2]);
    if(suffix > 0 && suffix < end-base && !strcmp(end-suffix, argv[2]))
      end[-suffix] = 0;
  }
  printf(1, "%s\n", base);
  exit();
}
