/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  char **p, *value;
  int i, status = 0;

  if(argc == 1){
    if(environ)
      for(p = environ; *p; p++)
        printf(1, "%s\n", *p);
    return 0;
  }
  for(i = 1; i < argc; i++){
    if((value = getenv(argv[i])))
      printf(1, "%s\n", value);
    else
      status = 1;
  }
  return status;
}
