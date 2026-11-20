/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mode.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "kassert.h"
#include "file.h"
#include "ext2.h"
#include "vfs.h"
#include "p9records.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// the inode cache gives processes a shared place to lock an inode
// keep the reference from iget while you still need the inode
// use ilock to load its metadata and protect it while you work
// drop the sleep lock with iunlock and release the reference with iput
//
// the cache spin lock protects the device inode number and reference count
// the inode sleep lock protects its remaining fields and contents

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

static struct sleeplock namespace;

void
iinit(void)
{
  int i = 0;
  
  initlock(&icache.lock, "icache");
  initsleeplock(&namespace, "namespace");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }

}

//PAGEBREAK!
// set the on disk type and return an unlocked reference to the new inode
struct inode*
ialloc(uint dev, short type)
{
  uint number = vfs_ops(dev)->alloc(dev, type);
  return number ? iget(dev, number) : 0;
}

int
iupdate(struct inode *ip)
{
  return vfs_ops(ip->dev)->save(ip);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&icache.lock);

  // Is the inode already cached?
  empty = 0;
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&icache.lock);

  return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
  acquire(&icache.lock);
  ip->ref++;
  release(&icache.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip)
{
  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if(!ip->valid){
    ip->type = 0;
    ip->nlink = 1;
    ip->flags = 0;
    vfs_ops(ip->dev)->load(ip);
    ip->valid = 1;
  }
}

// Unlock the given inode.
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void
iput(struct inode *ip)
{
  acquire(&icache.lock);
  KASSERT(ip->ref > 0);
  if(ip->ref == 1 && ip->valid && ip->nlink == 0){
    // the only reference is ours so taking the inode lock cannot sleep
    acquiresleep(&ip->lock);
    release(&icache.lock);

    vfs_ops(ip->dev)->drop(ip);
    ip->type = 0;
    ip->valid = 0;

    releasesleep(&ip->lock);
    acquire(&icache.lock);
  }
  ip->ref--;
  release(&icache.lock);
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

// Copy stat information from inode.
// caller must hold ip to lock
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

//PAGEBREAK!
// Read data from inode.
// caller must hold ip to lock
int
readi(struct inode *ip, char *dst, uint off, uint n)
{
  if(ip->type != T_DEV)
    return vfs_ops(ip->dev)->read(ip, dst, off, n);
  if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
    return -1;
  return devsw[ip->major].read(ip, dst, n, off, 0);
}

int
writei(struct inode *ip, char *src, uint off, uint n)
{
  if(ip->type != T_DEV)
    return vfs_ops(ip->dev)->write(ip, src, off, n);
  if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
    return -1;
  return devsw[ip->major].write(ip, src, n, 0);
}

//PAGEBREAK!
// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, P9_NAME_BUFSIZE);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint inum;

  if(dp->type != T_DIR)
    return 0;
  if(ext2host(dp->dev) && dp->inum == EXT2ROOT && !namecmp(name, ".."))
    return iget(ROOTDEV, ROOTINO);
  if(vfs_ops(dp->dev)->lookup(dp, name, &inum, poff) != 1)
    return 0;
  if(isroot(dp)){
    if(inum == ext2host(EXT2DEV))
      return iget(EXT2DEV, EXT2ROOT);
    if(inum == ext2host(INSTALLDEV))
      return iget(INSTALLDEV, EXT2ROOT);
  }
  return iget(dp->dev, inum);
}

int
dirlink(struct inode *dp, char *name, uint inum)
{
  struct inode *ip = dirlookup(dp, name, 0);

  if(ip){
    iput(ip);
    return -1;
  }
  return vfs_ops(dp->dev)->insert(dp, name, inum);
}

int
nextdent(struct inode *dp, uint *off, struct p9_direntx *out)
{
  memset(out, 0, sizeof(*out));
  return vfs_ops(dp->dev)->next(dp, off, out);
}

int
dirreplace(struct inode *dp, uint off, const char *name, uint inum)
{
  return vfs_ops(dp->dev)->replace(dp, off, name, inum);
}

int
isroot(struct inode *ip)
{
  return ip->dev == ROOTDEV && ip->inum == ROOTINO;
}

//PAGEBREAK!
// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = min(path - s, P9_NAME_MAX + 1);
  memmove(name, s, len);
  name[len] = 0;
  while(*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name which must have room for p9 name bufsize bytes
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int parent, int follow, char *name)
{
  struct inode *ip, *next;
  char *storage = 0, *expanded;
  uint hops = 0, used, rest;

  if(!*path)
    return 0;
  ip = *path == '/' ? iget(ROOTDEV, ROOTINO) : idup(myproc()->cwd);
  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR || !inodeaccess(ip, M_EXEC) ||
       strlen(name) > P9_NAME_MAX){
      iunlockput(ip);
      ip = 0;
      goto done;
    }
    if(parent && !*path){
      iunlock(ip);
      goto done;
    }
    next = dirlookup(ip, name, 0);
    iunlock(ip);
    if(!next)
      goto bad;

    if(next == ip){
      iput(ip);
      ip = next;
      continue;
    }
    ilock(next);
    if(!next->type){
      iunlockput(next);
      goto bad;
    }
    if(!(next->flags & I_SYMLINK) || (!follow && !*path)){
      iunlock(next);
      iput(ip);
      ip = next;
      continue;
    }

    // alternate two buffers so the remaining path stays readable
    if(!storage)
      storage = kalloc();
    if(!storage || hops >= 16 || !next->size || next->size >= P9_PATH_MAX){
      iunlockput(next);
      goto bad;
    }
    expanded = storage + (hops++ % 2) * P9_PATH_MAX;
    used = next->size;
    rest = strlen(path);
    if(used + !!rest + rest >= P9_PATH_MAX ||
       readi(next, expanded, 0, used) != used){
      iunlockput(next);
      goto bad;
    }
    iunlockput(next);
    if(rest){
      expanded[used++] = '/';
      memmove(expanded+used, path, rest);
    }
    expanded[used+rest] = 0;
    path = expanded;
    if(*path == '/'){
      iput(ip);
      ip = iget(ROOTDEV, ROOTINO);
    }
  }
  if(!parent)
    goto done;
bad:
  iput(ip);
  ip = 0;
done:
  if(storage)
    kfree(storage);
  return ip;
}

struct inode*
namei(char *path)
{
  char name[P9_NAME_BUFSIZE];
  return namex(path, 0, 1, name);
}

struct inode*
nameinofollow(char *path)
{
  char name[P9_NAME_BUFSIZE];
  return namex(path, 0, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, 1, name);
}

void namelock(void) { acquiresleep(&namespace); }
void nameunlock(void) { releasesleep(&namespace); }

void
statxi(struct inode *ip, struct statx *st)
{
  memset(st, 0, sizeof(*st));
  stati(ip, &st->st);
  st->uid = ip->uid;
  st->gid = ip->gid;
  st->mode = ip->mode;
}

// hold the inode lock while checking the callers access
int
inodeaccess(struct inode *ip, int need)
{
  struct proc *p = myproc();
  uint bits = ip->mode;

  if(!ip->type || (ip->flags & I_SHADOW))
    return 0;
  if(p->uid == 0)
    return ip->type != T_FILE || !(need & M_EXEC) || (bits & 0111);
  if(p->uid == ip->uid)
    bits >>= 6;
  else if(p->gid == ip->gid)
    bits >>= 3;
  return (bits & need) == need;
}


int
inodebusy(uint dev)
{
  struct inode *ip;
  int busy = 0;

  acquire(&icache.lock);
  for(ip = icache.inode; ip < icache.inode + NINODE; ip++)
    if(ip->ref && ip->dev == dev){
      busy = 1;
      break;
    }
  release(&icache.lock);
  return busy;
}
