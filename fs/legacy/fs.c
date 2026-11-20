/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mode.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "legacyfs.h"
#define ROOTINO LEGACY_ROOTINO
#include "fscheck.h"
#include "kassert.h"
#include "buf.h"
#include "file.h"
#include "ext2.h"
#include "vfs.h"
#include "p9records.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

// the filesystem uses one disk so one superblock is enough
struct superblock sb; 

// Read the super block.
void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Zero a block.
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
static uint
balloc(uint dev)
{
  int b, bi, m;
  uint first = sb.size - sb.nblocks;
  struct buf *bp;

  for(b = first - first % BPB; b < sb.size; b += BPB){
    bp = bread(dev, BBLOCK(b, sb));
    for(bi = b < first ? first - b : 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  return 0;
}

// Free a disk block.
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  KASSERT(b >= sb.size - sb.nblocks && b < sb.size);
  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a cache entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The icache.lock spin-lock protects the allocation of icache
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold icache.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

static struct sleeplock namespace;

void
iinit(int dev)
{
  int i = 0;
  
  initlock(&icache.lock, "icache");
  initsleeplock(&namespace, "namespace");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }

  readsb(dev, &sb);
  if(!fsgeometry(&sb))
    panic("invalid filesystem layout run the legacy fsck on the host");
  cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\
 inodestart %d bmap start %d\n", sb.size, sb.nblocks,
          sb.ninodes, sb.nlog, sb.logstart, sb.inodestart,
          sb.bmapstart);
}

//PAGEBREAK!
// set the on disk type and return an unlocked reference to the new inode
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  if(dev == EXT2DEV){
    uint number = vfs_ops(dev)->alloc(dev, type);
    return number ? iget(dev, number) : 0;
  }

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode*)bp->data + inum%IPB;
    if(dip->kind == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->kind = type << INODE_TYPE_SHIFT;
      log_write(bp);   // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  return 0;
}

// Copy a modified in-memory inode to disk.
// must be called after every change to an ip to xxx field
// that lives on disk since i node cache is write through
// caller must hold ip to lock
int
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if(ip->dev == EXT2DEV)
    return vfs_ops(ip->dev)->save(ip);
  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->kind = ip->type ? (ip->type << INODE_TYPE_SHIFT) | ip->mode | ip->flags : 0;
  dip->nlink = ip->nlink;
  dip->uid = ip->uid;
  dip->gid = ip->gid;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(dip->addrs));
  if(ip->type == T_DEV){
    dip->addrs[0] = (ushort)ip->major;
    dip->addrs[1] = (ushort)ip->minor;
  }
  log_write(bp);
  brelse(bp);
  return 0;
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
  struct buf *bp;
  struct dinode *dip;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if(!ip->valid && ip->dev == EXT2DEV){
    ip->type = 0;
    ip->nlink = 1;
    ip->flags = 0;
    vfs_ops(ip->dev)->load(ip);
    ip->valid = 1;
    return;
  }
  if(ip->valid == 0){
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->type = (dip->kind & INODE_TYPE_MASK) >> INODE_TYPE_SHIFT;
    ip->mode = dip->kind & M_VALID;
    ip->flags = dip->kind & (I_SHADOW | I_SYMLINK);
    ip->uid = dip->uid;
    ip->gid = dip->gid;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    if(ip->type == T_DEV){
      ip->major = dip->addrs[0];
      ip->minor = dip->addrs[1];
      memset(ip->addrs, 0, sizeof(ip->addrs));
    } else {
      ip->major = ip->minor = 0;
      memset(ip->addrs, 0, sizeof(ip->addrs));
      memmove(ip->addrs, dip->addrs, sizeof(dip->addrs));
    }
    brelse(bp);
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
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

    if(ip->dev == EXT2DEV){
      vfs_ops(ip->dev)->drop(ip);
      ip->type = 0;
    } else {
      itrunc(ip);
      ip->type = 0;
      iupdate(ip);
    }
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

//PAGEBREAK!
// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// look up the disk block for this part of the file
// reads leave missing blocks alone writes can ask bmap to allocate them
static uint
bmap(struct inode *ip, uint bn, int allocate)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0 && allocate)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr >= sb.size - sb.nblocks && addr < sb.size ? addr : 0;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // make sure a data block is available before allocating the indirect block
    if((addr = ip->addrs[NDIRECT]) == 0){
      uint data;

      if(!allocate || (data = balloc(ip->dev)) == 0)
        return 0;
      addr = balloc(ip->dev);
      if(addr == 0){
        bfree(ip->dev, data);
        return 0;
      }
      ip->addrs[NDIRECT] = addr;
      bp = bread(ip->dev, addr);
      ((uint*)bp->data)[bn] = data;
      log_write(bp);
      brelse(bp);
      return data;
    }
    if(addr < sb.size - sb.nblocks || addr >= sb.size)
      return 0;
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0 && allocate && (addr = balloc(ip->dev)) != 0){
      a[bn] = addr;
      log_write(bp);
    }
    brelse(bp);
    return addr >= sb.size - sb.nblocks && addr < sb.size ? addr : 0;
  }

  return 0;
}

// free the contents of an inode hold its lock inside a transaction
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
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
  uint tot, m, block;
  struct buf *bp;

  if(ip->dev == EXT2DEV)
    return vfs_ops(ip->dev)->read(ip, dst, off, n);
  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
      return -1;
    return devsw[ip->major].read(ip, dst, n, off, 0);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    block = bmap(ip, off/BSIZE, 0);
    if(!block)
      return -1;
    bp = bread(ip->dev, block);
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(dst, bp->data + off%BSIZE, m);
    brelse(bp);
  }
  return n;
}

// PAGEBREAK!
// Write data to inode.
// caller must hold ip to lock
int
writei(struct inode *ip, char *src, uint off, uint n)
{
  uint tot, m, block;
  struct buf *bp;

  if(ip->dev == EXT2DEV)
    return vfs_ops(ip->dev)->write(ip, src, off, n);
  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
      return -1;
    return devsw[ip->major].write(ip, src, n, 0);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off == MAXFILE*BSIZE && n)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    n = MAXFILE*BSIZE - off;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    block = bmap(ip, off/BSIZE, 1);
    if(!block)
      break;
    bp = bread(ip->dev, block);
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(bp->data + off%BSIZE, src, m);
    log_write(bp);
    brelse(bp);
  }

  if(tot > 0 && off > ip->size){
    ip->size = off;
    iupdate(ip);
  }
  return tot || n == 0 ? tot : -1;
}

//PAGEBREAK!
// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  if(dp->dev == EXT2DEV){
    if(dp->inum == EXT2ROOT && namecmp(name, "..") == 0)
      return iget(ROOTDEV, ROOTINO);
    if(vfs_ops(dp->dev)->lookup(dp, name, &inum, poff) == 1)
      return iget(dp->dev, inum);
    return 0;
  }
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      return 0;
    if(de.inum == 0 || de.inum >= sb.ninodes)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      if(dp->dev == ROOTDEV && dp->inum == ROOTINO && inum == ext2mountpoint())
        return iget(EXT2DEV, EXT2ROOT);
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  if(dp->dev == EXT2DEV)
    return vfs_ops(dp->dev)->insert(dp, name, inum);

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      return -1;
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    return -1;

  return 0;
}

int
nextdent(struct inode *dp, uint *off, struct p9_direntx *out)
{
  struct dirent de;

  memset(out, 0, sizeof(*out));
  if(dp->dev == EXT2DEV)
    return vfs_ops(dp->dev)->next(dp, off, out);
  if(*off % sizeof(de))
    return -1;
  while(*off < dp->size){
    if(readi(dp, (char*)&de, *off, sizeof(de)) != sizeof(de))
      return -1;
    *off += sizeof(de);
    if(!de.inum)
      continue;
    out->inum = de.inum;
    memmove(out->name, de.name, DIRSIZ);
    return 1;
  }
  return 0;
}

int
dirreplace(struct inode *dp, uint off, const char *name, uint inum)
{
  struct dirent de;

  if(dp->dev == EXT2DEV)
    return vfs_ops(dp->dev)->replace(dp, off, name, inum);
  memset(&de, 0, sizeof(de));
  de.inum = inum;
  if(name)
    strncpy(de.name, name, DIRSIZ);
  return writei(dp, (char*)&de, off, sizeof(de)) == sizeof(de) ? 0 : -1;
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
       (ip->dev == EXT2DEV && strlen(name) > P9_NAME_MAX)){
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
isroot(struct inode *ip)
{
  return ip->dev == ROOTDEV && ip->inum == ROOTINO;
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
