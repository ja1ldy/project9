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
    printf(1, "uid=%d(%s) gid=%d home=%s shell=/bin/sh\n", uid, info.name, getgid(), info.home);
  else
    printf(1, "uid=%d gid=%d\n", uid, getgid());
  exit();
}
