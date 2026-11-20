/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "account.h"

int
main(void)
{
  struct account_info info;
  int uid = getuid();

  if(accountinfo(uid, &info) == 0)
    printf(1, "%s\n", info.name);
  else
    printf(1, "%d\n", uid);
  exit();
}
