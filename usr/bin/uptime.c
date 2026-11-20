/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "p9abi.h"

int
main(int argc, char *argv[])
{
  int ticks = uptime();
  printf(1, "up %d sec (%d ticks)\n", ticks / P9_HZ, ticks);
  exit();
}
