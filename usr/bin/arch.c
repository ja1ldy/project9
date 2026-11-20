/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(void)
{
  printf(1, "%s\n", sizeof(uptr) == 8 ? "x86_64" : "i386");
  return 0;
}
