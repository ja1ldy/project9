/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

//
// File descriptors
//

#include "types.h"
#include "p9abi.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "stat.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "ext2.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
  localinit();
  ptyinit();
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      f->nonblock = 0;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// drop a reference and release the underlying resource on the last close
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_PTY)
    ptyclose(ff.pty, ff.ptymaster);
  else if(ff.type == FD_LOCAL)
    localclose(ff.local);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_PTY)
    return ptymetadata(f, st);
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -1;
  if(n == 0)
    return 0;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n, f->nonblock);
  if(f->type == FD_LOCAL)
    return localio(f->local, addr, n, f->nonblock, 0);
  if(f->type == FD_PTY)
    return ptyread(f->pty, f->ptymaster, addr, n, f->nonblock);
  if(f->type == FD_INODE){
    ilock(f->ip);
#ifdef P9_LEGACY_ROOT
    if(f->ip->type == T_DIR && f->ip->dev == EXT2DEV)
#else
    if(f->ip->type == T_DIR)
#endif
      r = -1;
    else if(f->ip->type == T_DEV && f->ip->major >= 0 && f->ip->major < NDEV &&
       devsw[f->ip->major].read)
      r = devsw[f->ip->major].read(f->ip, addr, n, f->off, f->nonblock);
    else
      r = readi(f->ip, addr, f->off, n);
    if(r > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(n == 0)
    return 0;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n, f->nonblock);
  if(f->type == FD_LOCAL)
    return localio(f->local, addr, n, f->nonblock, 1);
  if(f->type == FD_PTY)
    return ptywrite(f->pty, f->ptymaster, addr, n, f->nonblock);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if(f->ip->type == T_DEV){
      r = f->ip->major >= 0 && f->ip->major < NDEV && devsw[f->ip->major].write ?
          devsw[f->ip->major].write(f->ip, addr, n, f->nonblock) : -1;
      iunlock(f->ip);
      return r;
    }
    iunlock(f->ip);
    // let other filesystem operations run between large writes
    int max = 8192;
#ifdef P9_LEGACY_ROOT
    if(f->ip->dev == ROOTDEV)
      max = 1536;
#endif
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if(f->append)
        f->off = f->ip->size;
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r > 0)
        i += r;
      if(r != n1)
        break;
    }
    return i ? i : -1;
  }
  panic("filewrite");
}


int
fileready(struct file *f, int writing)
{
  struct inode *ip;
  int ready = 1, major;

  if(writing ? !f->writable : !f->readable)
    return 0;
  if(f->type == FD_PIPE)
    return pipeready(f->pipe, writing);
  if(f->type == FD_LOCAL)
    return localready(f->local, writing);
  if(f->type == FD_PTY)
    return ptyready(f->pty, f->ptymaster, writing);
  ip = f->ip;
  ilock(ip);
  major = ip->type == T_DEV ? ip->major : -1;
  iunlock(ip);
  if(major >= 0 && major < NDEV && devsw[major].ready)
    ready = devsw[major].ready(ip, writing);
  return ready;
}

int
filepoll(struct file *f, int wanted)
{
  int flags = 0;
  if((wanted & POLLIN) && fileready(f, 0))
    flags |= POLLIN;
  if((wanted & POLLOUT) && fileready(f, 1))
    flags |= POLLOUT;
  if(f->type == FD_PTY)
    flags |= ptyevents(f->pty, f->ptymaster);
  else if(f->type == FD_LOCAL)
    flags |= localevents(f->local);
  else if(f->type == FD_PIPE)
    flags |= pipeevents(f->pipe, f->writable);
  return flags;
}
