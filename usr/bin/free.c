/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "memlayout.h"

int
main(int argc, char *argv[])
{
  int free_bytes = freemem();
  int total_bytes = memlimit();
  int used_bytes = total_bytes - free_bytes;

  printf(1, "              total        used        free\n");
  printf(1, "Mem (KB):   %d      %d       %d\n",
         total_bytes / 1024, used_bytes / 1024, free_bytes / 1024);
  exit();
}
