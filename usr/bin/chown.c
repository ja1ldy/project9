/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "mode.h"

int
main(int argc, char **argv)
{
  char *owner, *group;
  uint uid = -1, gid = -1;
  int i, failed = 0;

  if(argc < 3)
    goto usage;
  owner = argv[1];
  group = strchr(owner, ':');
  if(group){
    *group++ = 0;
    if(parseuint(group, &gid) < 0 || gid > ID_MAX)
      goto usage;
  }
  if(*owner){
    if(parseuint(owner, &uid) < 0 || uid > ID_MAX)
      goto usage;
  } else if(!group){
    goto usage;
  }
  for(i = 2; i < argc; i++){
    if(chown(argv[i], uid, gid) < 0){
      printf(2, "chown: %s: cannot change ownership\n", argv[i]);
      failed = 1;
    }
  }
  exitstatus(failed);

usage:
  printf(2, "usage: chown uid[:gid] file ... | chown :gid file ...\n");
  exitstatus(1);
}
