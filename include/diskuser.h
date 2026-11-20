/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_DISKUSER_H
#define P9_DISKUSER_H

#include "types.h"
#include "user.h"
#include "diskctl.h"

int diskctl(int, int, int, int, void *);
int installmount(int, int);
int disksync(void);
int symlink(const char *, const char *);
int fchmod(int, int);

static inline int
diskid(const char *name, int part)
{
  uint id = 0;
  int sata;

  if(strlen(name) >= 5 && name[0] == '/' && name[1] == 'd' && name[2] == 'e' && name[3] == 'v' && name[4] == '/')
    name += 5;
  if((name[0] != 'h' && name[0] != 's') || name[1] != 'd')
    return -1;
  sata = name[0] == 's';
  name += 2;
  if(*name < '0' || *name > '9')
    return -1;
  for(; *name >= '0' && *name <= '9'; name++){
    id = 10*id + *name-'0';
    if(id >= DC_MAX)
      return -1;
  }
  if(part ? (strcmp(name, "a") && strcmp(name, "p1")) : *name)
    return -1;
  return sata ? (id < DC_MAX-4 ? id+4 : -1) : id < 4 ? id : -1;
}

static inline int
dcompare(const void *a, const void *b, uint n)
{
  const uchar *x = a, *y = b;
  while(n--){
    if(*x++ != *y++)
      return 1;
  }
  return 0;
}

static inline uint
dget(const uchar *p, uint n)
{
  uint a = 0;
  for(uint j = 0; j < n; j++)
    a |= (uint)p[j] << (j*8);
  return a;
}

static inline void
dput(uchar *p, uint v, uint n)
{
  for(uint j = 0; j < n; j++)
    p[j] = v >> (j*8);
}

#endif
