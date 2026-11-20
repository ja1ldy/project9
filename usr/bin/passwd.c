/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "account.h"
#include "tty.h"

int
main(int argc, char **argv)
{
  char old[ACCOUNT_PASSWORD] = {0}, new[ACCOUNT_PASSWORD] = {0};
  int uid = getuid();

  if(argc > 2 || (argc == 2 && uid != 0)){
    printf(2, "usage: passwd [user] (only root may name a user)\n");
    exit();
  }
  signal(SIGQUIT, SIG_IGN);
  if(uid != 0 && readpassword("Current password: ", old) < 0)
    goto done;
  if(newpassword(new) < 0)
    goto done;
  if(account(ACCOUNT_CHANGE, argc == 2 ? argv[1] : "", old, new) == 0)
    printf(1, "Password updated.\n");
  else
    printf(2, "passwd: password update failed\n");
done:
  scrub(old, sizeof(old));
  scrub(new, sizeof(new));
  exit();
}
