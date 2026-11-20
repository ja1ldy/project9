/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "p9abi.h"
#include "local.h"

enum { backlog_max = 8 };
enum { free_slot, fresh, binding, bound, listening, connected };

struct local {
  int state, family, port, limit, count, head;
  struct inode *name;
  struct file *rx, *tx;
  struct local *pending[backlog_max];
};

static struct spinlock lock;
static struct local sockets[NFILE];

void
localinit(void)
{
  initlock(&lock, "local sockets");
}

static struct local *
slot(void)
{
  for(struct local *s = sockets; s < sockets + NFILE; s++){
    if(s->state == free_slot){
      memset(s, 0, sizeof(*s));
      s->state = fresh;
      return s;
    }
  }
  return 0;
}

int
localalloc(struct file *f, int family)
{
  acquire(&lock);
  struct local *s = slot();
  release(&lock);
  if(!s)
    return -P9_ENFILE;
  s->family = family;
  f->local = s;
  f->readable = f->writable = 1;
  f->type = FD_LOCAL;
  return 0;
}

int
localreserve(struct local *s)
{
  acquire(&lock);
  int good = s->state == fresh;
  if(good)
    s->state = binding;
  release(&lock);
  return good ? 0 : -P9_EINVAL;
}

void
localname(struct local *s, struct inode *name)
{
  acquire(&lock);
  s->name = name;
  s->state = name ? bound : fresh;
  release(&lock);
}

int
localfamily(struct local *s)
{
  return s->family;
}

int
localport(struct local *s, int port)
{
  acquire(&lock);
  int result = -P9_EINVAL;
  if(s->state == fresh){
    result = 0;
    for(struct local *p = sockets; p < sockets + NFILE; p++)
      if((p->state == bound || p->state == listening) && p->port == port){
        result = -P9_EADDRINUSE;
        break;
      }
    if(!result){
      s->port = port;
      s->state = bound;
    }
  }
  release(&lock);
  return result;
}

static void
closepair(struct local *s)
{
  if(s->rx)
    fileclose(s->rx);
  if(s->tx)
    fileclose(s->tx);
}

void
localclose(struct local *s)
{
  struct inode *name;
  acquire(&lock);
  while(s->count){
    struct local *peer = s->pending[s->head];
    s->head = (s->head + 1) % backlog_max;
    s->count--;
    closepair(peer);
    peer->state = free_slot;
  }
  closepair(s);
  name = s->name;
  s->state = free_slot;
  wakeup(s);
  selectnotify();
  release(&lock);
  if(name){
    begin_op();
    iput(name);
    end_op();
  }
}

int
locallisten(struct local *s, int backlog)
{
  acquire(&lock);
  int good = s->state == bound || s->state == listening;
  if(good){
    s->state = listening;
    s->limit = backlog > backlog_max ? backlog_max : backlog < 1 ? 1 : backlog;
  }
  release(&lock);
  return good ? 0 : -P9_EINVAL;
}

int
localconnect(struct local *s, struct inode *name, int port)
{
  struct file *sr = 0, *sw = 0, *cr = 0, *cw = 0;
  if(localreserve(s) < 0)
    return -P9_EISCONN;
  int result = -P9_ENFILE;
  if(pipealloc(&sr, &cw) < 0){
    sr = cw = 0;
    goto done;
  }
  if(pipealloc(&cr, &sw) < 0){
    cr = sw = 0;
    goto done;
  }

  acquire(&lock);
  result = -P9_ECONNREFUSED;
  for(struct local *listener = sockets; listener < sockets + NFILE; listener++){
    if(listener->state != listening || listener->family != s->family ||
       listener->name != name || listener->port != port)
      continue;
    if(listener->count >= listener->limit){
      result = -P9_EAGAIN;
      break;
    }
    struct local *peer = slot();
    if(!peer){
      result = -P9_ENFILE;
      break;
    }
    s->rx = cr;
    s->tx = cw;
    peer->family = s->family;
    peer->rx = sr;
    peer->tx = sw;
    s->state = peer->state = connected;
    listener->pending[(listener->head + listener->count++) % backlog_max] = peer;
    wakeup(listener);
    selectnotify();
    result = 0;
    break;
  }
  release(&lock);

done:
  if(result){
    localname(s, 0);
    if(sr) fileclose(sr);
    if(sw) fileclose(sw);
    if(cr) fileclose(cr);
    if(cw) fileclose(cw);
  }
  return result;
}

int
localaccept(struct file *listener, struct file *new)
{
  struct local *s = listener->local;
  acquire(&lock);
  if(s->state != listening){
    release(&lock);
    return -P9_EINVAL;
  }
  while(!s->count){
    if(listener->nonblock || sleepintr(s, &lock) < 0){
      release(&lock);
      return listener->nonblock ? -P9_EAGAIN : -P9_EINTR;
    }
  }
  new->local = s->pending[s->head];
  s->head = (s->head + 1) % backlog_max;
  s->count--;
  new->readable = new->writable = 1;
  new->type = FD_LOCAL;
  release(&lock);
  return 0;
}


int
localshutdown(struct local *s, int how)
{
  struct file *rx = 0, *tx = 0;
  if(how < SHUT_RD || how > SHUT_RDWR)
    return -P9_EINVAL;
  acquire(&lock);
  if(s->state != connected){
    release(&lock);
    return -P9_ENOTCONN;
  }
  if(how != SHUT_WR){
    rx = s->rx;
    s->rx = 0;
  }
  if(how != SHUT_RD){
    tx = s->tx;
    s->tx = 0;
  }
  release(&lock);
  if(rx) fileclose(rx);
  if(tx) fileclose(tx);
  return 0;
}

int
localio(struct local *s, char *data, int n, int nonblock, int writing)
{
  acquire(&lock);
  if(s->state != connected){
    release(&lock);
    return -P9_ENOTCONN;
  }
  struct file *end = writing ? s->tx : s->rx;
  if(end)
    filedup(end);
  release(&lock);
  if(!end)
    return writing ? -P9_EPIPE : 0;
  int result = writing ? pipewrite(end->pipe, data, n, nonblock) :
                         piperead(end->pipe, data, n, nonblock);
  fileclose(end);
  return result;
}

int
localready(struct local *s, int writing)
{
  acquire(&lock);
  int ready = 0;
  if(s->state == listening)
    ready = !writing && s->count;
  if(s->state == connected){
    struct file *end = writing ? s->tx : s->rx;
    ready = !end || pipeready(end->pipe, writing);
  }
  release(&lock);
  return ready;
}

int
localqueued(struct local *s)
{
  acquire(&lock);
  int count = -P9_ENOTCONN;
  if(s->state == connected)
    count = s->rx ? pipequeued(s->rx->pipe) : 0;
  release(&lock);
  return count;
}

int
localevents(struct local *s)
{
  acquire(&lock);
  int flags = s->state == connected ?
    (s->rx ? pipeevents(s->rx->pipe, 0) : POLLHUP) |
    (s->tx ? pipeevents(s->tx->pipe, 1) : POLLERR) : 0;
  release(&lock);
  return flags;
}
