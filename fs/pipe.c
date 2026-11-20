/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "p9abi.h"

#define PIPESIZE 512

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read
  uint nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};

int
pipequeued(struct pipe *p)
{
  acquire(&p->lock);
  int count = p->nwrite - p->nread;
  release(&p->lock);
  return count;
}

int
pipealloc(struct file **f0, struct file **f1)
{
  struct pipe *p;

  p = 0;
  *f0 = *f1 = 0;
  if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
    goto bad;
  if((p = (struct pipe*)kalloc()) == 0)
    goto bad;
  p->readopen = 1;
  p->writeopen = 1;
  p->nwrite = 0;
  p->nread = 0;
  initlock(&p->lock, "pipe");
  (*f0)->type = FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = p;
  (*f1)->type = FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = p;
  return 0;

//PAGEBREAK: 20
 bad:
  if(p)
    kfree((char*)p);
  if(*f0)
    fileclose(*f0);
  if(*f1)
    fileclose(*f1);
  return -1;
}

void
pipeclose(struct pipe *p, int writable)
{
  acquire(&p->lock);
  if(writable){
    p->writeopen = 0;
    wakeup(&p->nread);
  } else {
    p->readopen = 0;
    wakeup(&p->nwrite);
  }
  selectnotify();
  if(p->readopen == 0 && p->writeopen == 0){
    release(&p->lock);
    kfree((char*)p);
  } else
    release(&p->lock);
}

//PAGEBREAK: 40
int
pipewrite(struct pipe *p, char *addr, int n, int nonblock)
{
  int i, error;

  acquire(&p->lock);
  for(i = 0; i < n; i++){
    if(!p->readopen){
      release(&p->lock);
      return i ? i : -P9_EPIPE;
    }
    while(p->nwrite == p->nread + PIPESIZE){  //DOC: pipewrite-full
      if(p->readopen == 0 || myproc()->killed){
        error = p->readopen ? P9_EINTR : P9_EPIPE;
        release(&p->lock);
        return i ? i : -error;
      }
      wakeup(&p->nread);
      selectnotify();
      if(nonblock){
        release(&p->lock);
        return i ? i : -P9_EAGAIN;
      }
      if(sleepintr(&p->nwrite, &p->lock) < 0){
        release(&p->lock);
        return i ? i : -1;
      }
    }
    if(!p->readopen){
      release(&p->lock);
      return i ? i : -P9_EPIPE;
    }
    p->data[p->nwrite++ % PIPESIZE] = addr[i];
  }
  wakeup(&p->nread);
  selectnotify();
  release(&p->lock);
  return n;
}

int
piperead(struct pipe *p, char *addr, int n, int nonblock)
{
  int i;

  acquire(&p->lock);
  while(p->nread == p->nwrite && p->writeopen){  //DOC: pipe-empty
    if(nonblock){
      release(&p->lock);
      return -P9_EAGAIN;
    }
    if(myproc()->killed){
      release(&p->lock);
      return -1;
    }
    if(sleepintr(&p->nread, &p->lock) < 0){
      release(&p->lock);
      return -1;
    }
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(p->nread == p->nwrite)
      break;
    addr[i] = p->data[p->nread++ % PIPESIZE];
  }
  wakeup(&p->nwrite);
  if(i)
    selectnotify();
  release(&p->lock);
  return i;
}

int
pipeready(struct pipe *p, int writing)
{
  int ready;

  acquire(&p->lock);
  ready = writing ? !p->readopen || p->nwrite - p->nread < PIPESIZE :
                    !p->writeopen || p->nread != p->nwrite;
  release(&p->lock);
  return ready;
}

int
pipeevents(struct pipe *p, int writing)
{
  acquire(&p->lock);
  int flags = writing ? (p->readopen ? 0 : POLLERR) : (p->writeopen ? 0 : POLLHUP);
  release(&p->lock);
  return flags;
}
