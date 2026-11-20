/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "param.h"
#include "procinfo.h"

static struct procinfo table[NPROC];

int
main(int argc, char **argv)
{
  uint pid;
  int n, i, j, found = 0;

  for(j = 1; j < argc; j++){
    if(parseuint(argv[j], &pid) < 0 || !pid){
      printf(2, "usage: ps [pid ...]\n");
      return 2;
    }
  }
  n = getprocs(table, NPROC);
  if(n < 0){
    printf(2, "ps: cannot read process table\n");
    return 1;
  }
  printf(1, "PID PPID PGID UID GID STAT SIZE COMMAND\n");
  for(i = 0; i < n; i++){
    const struct procinfo *p = &table[i];
    for(j = 1; j < argc; j++){
      parseuint(argv[j], &pid);
      if(pid == (uint)p->pid)
        break;
    }
    if(argc > 1 && j == argc)
      continue;
    printf(1, "%d %d %d %d %d %c %u %s\n",
           p->pid, p->ppid, p->pgid, p->uid, p->gid, p->state, p->size, p->name);
    found = 1;
  }
  return !found;
}
