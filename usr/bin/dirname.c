/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  char *path, *end;

  if(argc != 2){
    printf(2, "usage: dirname path\n");
    exit();
  }
  path = argv[1];
  end = path + strlen(path);
  while(end > path && end[-1] == '/')
    end--;
  while(end > path && end[-1] != '/')
    end--;
  if(end == path){
    printf(1, "%s\n", *path == '/' ? "/" : ".");
    exit();
  }
  while(end > path+1 && end[-1] == '/')
    end--;
  *end = 0;
  printf(1, "%s\n", path);
  exit();
}
