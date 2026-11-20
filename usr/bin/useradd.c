/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "account.h"
#include "tty.h"
#include "mode.h"

int
main(int argc, char **argv)
{
  char password[ACCOUNT_PASSWORD] = {0};
  int status = 1;

  const char *group = "";
  uint gid;

  if(argc == 4 && !strcmp(argv[1], "-g") &&
     parseuint(argv[2], &gid) == 0 && gid > 0 && gid <= ID_MAX)
    group = argv[2];
  else if(argc != 2){
    printf(2, "usage: useradd [-g gid] user (root only)\n");
    exitstatus(1);
  }
  if(getuid() != 0){
    printf(2, "useradd: only root can create an account\n");
    exitstatus(1);
  }
  signal(SIGQUIT, SIG_IGN);
  if(newpassword(password) == 0){
    if(account(ACCOUNT_ADD, argv[argc-1], group, password) == 0){
      printf(1, "Created %s.\n", argv[argc-1]);
      status = 0;
    } else
      printf(2, "useradd: cannot create %s\n", argv[argc-1]);
  }
  scrub(password, sizeof(password));
  exitstatus(status);
}
