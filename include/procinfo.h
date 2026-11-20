/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_PROCINFO_H
#define PROJECT9_PROCINFO_H

struct procinfo {
  int pid, ppid, pgid, uid, gid;
  unsigned int size;
  char state;
  char name[16];
};

int getprocs(struct procinfo *, int);

#endif
