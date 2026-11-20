/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_SELECT_H
#define P9_LIBC_SYS_SELECT_H
#include <project9/p9abi.h>
static inline void p9_fdzero(fd_set *s) {
  for(unsigned i=0; i<FD_SETSIZE/32; i++) s->bits[i]=0;
}
static inline void p9_fdset(int n, fd_set *s) {
  if((unsigned)n<FD_SETSIZE) s->bits[n/32]|=1U<<(n%32);
}
static inline void p9_fdclr(int n, fd_set *s) {
  if((unsigned)n<FD_SETSIZE) s->bits[n/32]&=~(1U<<(n%32));
}
static inline int p9_fdisset(int n, const fd_set *s) {
  return (unsigned)n<FD_SETSIZE && (s->bits[n/32] & (1U<<(n%32)));
}
#define FD_ZERO(s) p9_fdzero(s)
#define FD_SET(n,s) p9_fdset(n,s)
#define FD_CLR(n,s) p9_fdclr(n,s)
#define FD_ISSET(n,s) p9_fdisset(n,s)
int select(int, fd_set *, fd_set *, fd_set *, struct timeval *) __asm__("p9_select");
#endif
