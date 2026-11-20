/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "p9abi.h"

static uint readyseq;

void
selectnotify(void)
{
  acquire(&tickslock);
  readyseq++;
  wakeup(&readyseq);
  wakeup(&ticks);
  release(&tickslock);
}

static int
getset(int arg, fd_set **set, fd_set *wanted)
{
  int addr;

  memset(wanted, 0, sizeof(*wanted));
  if(argint(arg, &addr) < 0 || (addr &&
     argptrwrite(arg, (char**)set, sizeof(*wanted)) < 0))
    return -1;
  if(addr)
    *wanted = **set;
  else
    *set = 0;
  return 0;
}

int
sys_select(void)
{
  fd_set *rd = 0, *wr = 0, *ex = 0, readset, writeset, exceptset, r, w;
  struct proc *p = myproc();
  struct timeval *tv;
  uint start, now, seq, duration = 0, bit, word;
  int nfds, addr, fd, count;

  if(argint(0, &nfds) < 0 || nfds < 0 || nfds > FD_SETSIZE ||
     getset(1, &rd, &readset) < 0 || getset(2, &wr, &writeset) < 0 ||
     getset(3, &ex, &exceptset) < 0 || argint(4, &addr) < 0)
    return -P9_EINVAL;
  if(addr){
    if(argptr(4, (char**)&tv, sizeof(*tv)) < 0)
      return -P9_EFAULT;
    if(tv->tv_sec < 0 || tv->tv_usec < 0 || tv->tv_usec >= 1000000 ||
       (uint)tv->tv_sec > (0x7fffffffU - P9_HZ) / P9_HZ)
      return -P9_EINVAL;
    duration = (uint)tv->tv_sec * P9_HZ +
               (tv->tv_usec + 1000000/P9_HZ - 1) / (1000000/P9_HZ);
    if(duration)
      duration++;
  }
  for(fd = 0; fd < nfds; fd++)
    if(((readset.bits[fd/32] | writeset.bits[fd/32] | exceptset.bits[fd/32]) &
        (1U << (fd%32))) && (fd >= NOFILE || !p->ofile[fd]))
      return -P9_EBADF;

  acquire(&tickslock);
  start = ticks;
  release(&tickslock);
  for(;;){
    acquire(&tickslock);
    now = ticks;
    seq = readyseq;
    release(&tickslock);
    memset(&r, 0, sizeof(r));
    memset(&w, 0, sizeof(w));
    count = 0;
    for(fd = 0; fd < nfds; fd++){
      word = fd / 32;
      bit = 1U << (fd % 32);
      if((readset.bits[word] & bit) && fileready(p->ofile[fd], 0)){
        r.bits[word] |= bit;
        count++;
      }
      if((writeset.bits[word] & bit) && fileready(p->ofile[fd], 1)){
        w.bits[word] |= bit;
        count++;
      }
    }
    if(count || (addr && now - start >= duration))
      break;

    // devices can change after the scan but before this wait starts
    acquire(&tickslock);
    if(readyseq == seq && (!addr || ticks == now) &&
       sleepintr(addr ? &ticks : &readyseq, &tickslock) < 0){
      release(&tickslock);
      return -P9_EINTR;
    }
    release(&tickslock);
  }
  if(rd)
    *rd = r;
  if(wr)
    *wr = w;
  if(ex)
    memset(ex, 0, sizeof(*ex));
  return count;
}

int
sys_gettimeofday(void)
{
  struct timeval *tv = 0, now;
  struct timezone *tz = 0;
  int addr, zone;

  if(argint(0, &addr) < 0 || argint(1, &zone) < 0 ||
     (addr && argptrwrite(0, (char**)&tv, sizeof(*tv)) < 0) ||
     (zone && argptrwrite(1, (char**)&tz, sizeof(*tz)) < 0))
    return -P9_EFAULT;
  if(addr){
    if(rtcgettime(&now) < 0)
      return -P9_EIO;
    *tv = now;
  }
  if(zone)
    memset(tz, 0, sizeof(*tz));
  return 0;
}

int
sys_setsid(void)
{
  int sid = newsession();
  return sid < 0 ? -P9_EPERM : sid;
}

int
sys_sigmask(void)
{
  int mode, mask;
  if(argint(0, &mode) < 0 || argint(1, &mask) < 0 || mode < -1 || mode > 2)
    return -P9_EINVAL;
  return signalmask(mode, mask);
}

int
sys_sigactionx(void)
{
  int sig, handler, tramp, mask, addr;
  uint *old = 0;

  if(argint(0, &sig) < 0 || argint(1, &handler) < 0 ||
     argint(2, &tramp) < 0 || argint(3, &mask) < 0 || argint(4, &addr) < 0 ||
     (addr && argptrwrite(4, (char**)&old, sizeof(*old)) < 0))
    return -P9_EINVAL;
  return setsignalx(sig, handler, tramp, mask, old);
}

int
sys_poll(void)
{
  struct proc *p = myproc();
  struct pollfd *item = 0;
  int n, timeout;
  if(argint(1, &n) < 0 || n < 0 || n > FD_SETSIZE ||
     argint(2, &timeout) < 0 || (n && argptrwrite(0, (char**)&item, n * sizeof(*item)) < 0))
    return -P9_EINVAL;

  uint duration = timeout > 0 ? ((uint)timeout + (1000 / P9_HZ) - 1) / (1000 / P9_HZ) + 1 : 0;
  acquire(&tickslock);
  uint start = ticks;
  release(&tickslock);
  for(;;){
    acquire(&tickslock);
    uint now = ticks, seq = readyseq;
    release(&tickslock);
    int ready = 0;
    for(int i = 0; i < n; i++){
      int fd = item[i].fd;
      item[i].revents = fd < 0 ? 0 : fd >= NOFILE || !p->ofile[fd] ? POLLNVAL :
        filepoll(p->ofile[fd], item[i].events);
      ready += !!item[i].revents;
    }
    if(ready || (timeout >= 0 && now - start >= duration))
      return ready;
    acquire(&tickslock);
    if(seq == readyseq && (timeout < 0 || now == ticks) &&
       sleepintr(timeout < 0 ? &readyseq : &ticks, &tickslock) < 0){
      release(&tickslock);
      return -P9_EINTR;
    }
    release(&tickslock);
  }
}
