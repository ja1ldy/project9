/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  return write(1, "\033[2J\033[H", 7) != 7;
}
