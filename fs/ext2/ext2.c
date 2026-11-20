/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "fs.h"
#include "stat.h"
#include "mode.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "ext2.h"
#include "vfs.h"
#include "disk.h"
#include "diskctl.h"
#include "p9abi.h"

#define E2_MAGIC 0xef53
#define E2_VALID 1
#define E2_DEV 0020000
#define E2_DIR 0040000
#define E2_REG 0100000
#define E2_LNK 0120000
#define E2_SOCK 0140000
#define E2_KIND 0170000
#define E2_FILETYPE 2
#define E2_JOURNAL 4
#define E2_IMMUTABLE 0x10
#define E2_APPEND 0x20
#define E2_INDEXED 0x1000
#define E2_DIRECT 12
#define SECTOR 512

struct super {
  uint inodes, blocks, reserved_blocks, free_blocks, free_inodes;
  uint first_data, log_block, log_frag, blocks_per_group, frags_per_group, inodes_per_group;
  uint mtime, wtime;
  ushort mounts, max_mounts, magic, state, errors, minor_rev;
  uint lastcheck, checkinterval, creator, rev;
  ushort reserved_uid, reserved_gid;
  uint first_inode;
  ushort inode_size, group;
  uint compat, incompat, ro_compat;
};

struct group {
  uint blockmap, inodemap, table;
  ushort free_blocks, free_inodes, used_dirs, pad;
  uint reserved[3];
};

struct dinode2 {
  ushort mode, uid;
  uint size, atime, ctime, mtime, dtime;
  ushort gid, links;
  uint sectors, flags, osd1, blocks[15];
  uint generation, acl, size_high, frag;
  uchar osd2[12];
};

struct dent {
  uint inum;
  ushort len;
  uchar name_len, type;
};

_Static_assert(sizeof(struct super) == 104, "ext2 superblock layout");
_Static_assert(sizeof(struct group) == 32, "ext2 group descriptor layout");
_Static_assert(sizeof(struct dinode2) == 128, "ext2 inode layout");
_Static_assert(sizeof(struct dent) == 8, "ext2 directory header layout");

static struct sleeplock lock;
struct volume {
  struct super sb;
  uint dev, capacity, sectors, blocksize, groups, tablesize, inodebytes, firstino;
  uint orphans;
  int writable, faulted, dirty;

  struct sectorbuf {
    uint tag;
    int valid;
    uchar data[SECTOR];
  } cache[257];
};

static struct volume disks[INSTALLDEV+1];
static struct volume *volume;
static struct ext2stats stats;
static uint mountino, shadowino, installino;

static const struct fsops ext2_ops = {
  .alloc = ext2alloc, .load = ext2load, .save = ext2save, .drop = ext2free,
  .read = ext2read, .write = ext2write, .truncate = ext2truncate,
  .writable = ext2maywrite, .lookup = ext2lookup, .insert = ext2insert,
  .replace = ext2setentry, .next = ext2next,
};

static int putsuper(void);
static int ext2writable(void);

static int
corrupt(const char *what, uint inode)
{
  if(!volume->faulted)
    cprintf("ext2: volume %d invalid %s at inode %d\n", volume->dev, what, inode);
  volume->faulted = 1;
  return -1;
}

static uint
min(uint a, uint b)
{
  return a < b ? a : b;
}

static uint
clockseconds(void)
{
  struct timeval t;
  return rtcgettime(&t) == 0 ? t.tv_sec : 0;
}

static struct sectorbuf *
cached(uint sector)
{
  return &volume->cache[sector % NELEM(volume->cache)];
}

static int
matches(struct sectorbuf *entry, uint sector)
{
  return entry->valid && entry->tag == sector;
}

static void
remember(struct sectorbuf *entry, uint sector, const uchar *data)
{
  memmove(entry->data, data, SECTOR);
  entry->tag = sector;
  entry->valid = 1;
}

// the ext2 lock serializes the cache and keeps sector writes in submission order
static int
diskio(uint sector, uint offset, void *buf, uint n, int write)
{
  uchar *p = buf;

  if(!n)
    return 0;
  if(offset/SECTOR >= volume->capacity || sector >= volume->capacity - offset/SECTOR)
    return -1;
  sector += offset / SECTOR;
  offset %= SECTOR;
  if((n-1)/SECTOR + ((n-1)%SECTOR + offset)/SECTOR >= volume->capacity-sector)
    return -1;

  if(write){
    if(!volume->writable || volume->faulted)
      return -1;
    if(!volume->dirty){
      // persist the dirty flag before changing the filesystem
      volume->dirty = 1;
      volume->sb.state &= ~E2_VALID;
      if(putsuper() < 0 || diskflush(volume->dev) < 0)
        goto fault;
    }
  }

  while(n){
    struct sectorbuf *entry = cached(sector);
    uint bytes = min(n, SECTOR-offset), batch = 1;
    int hit = matches(entry, sector);

    if(write && hit && !memcmp(entry->data+offset, p, bytes)){
      stats.skipped_writes++;
    } else if(bytes == SECTOR && (write || !hit)){
      uint limit = min(n/SECTOR, DISK_BATCH);
      while(batch < limit && (write || !matches(cached(sector+batch), sector+batch)))
        batch++;
      if(diskrw(volume->dev, sector, batch, p, write) < 0){
        if(write)
          for(uint i = 0; i < batch; i++)
            cached(sector+i)->valid = 0;
        goto fault;
      }
      for(uint i = 0; i < batch; i++)
        remember(cached(sector+i), sector+i, p+i*SECTOR);
      if(!write)
        stats.misses += batch;
      bytes = batch*SECTOR;
    } else {
      if(!hit){
        entry->valid = 0;
        if(diskrw(volume->dev, sector, 1, entry->data, 0) < 0)
          goto fault;
        entry->tag = sector;
        entry->valid = 1;
        stats.misses++;
      } else {
        stats.hits++;
      }
      if(write){
        memmove(entry->data+offset, p, bytes);
        if(diskrw(volume->dev, sector, 1, entry->data, 1) < 0){
          entry->valid = 0;
          goto fault;
        }
      } else {
        memmove(p, entry->data+offset, bytes);
      }
    }
    sector += batch;
    offset = 0;
    p += bytes;
    n -= bytes;
  }
  return 0;

fault:
  if(!volume->faulted)
    cprintf("ext2: volume %d %s failed at sector 0x%x\n",
            volume->dev, write ? "write or flush" : "read", sector);
  volume->faulted = 1;
  return -1;
}

void
ext2stats(struct ext2stats *out)
{
  acquiresleep(&lock);
  *out = stats;
  releasesleep(&lock);
}

static int
blockio(uint block, uint off, void *buf, uint n, int write)
{
  if(block >= volume->sb.blocks || off > volume->blocksize || n > volume->blocksize-off)
    return -1;
  return diskio(block * volume->sectors, off, buf, n, write);
}

static uint
groupbase(uint group)
{
  return volume->sb.first_data + group*volume->sb.blocks_per_group;
}

static int
powerof(uint n, uint base)
{
  while(n > 1 && n % base == 0)
    n /= base;
  return n == 1;
}

static int
groupio(uint num, struct group *g, int write)
{
  uint start, end;

  if(num >= volume->groups || diskio((volume->sb.first_data+1)*volume->sectors,
                             num*sizeof(*g), g, sizeof(*g), write) < 0)
    return -1;
  start = groupbase(num);
  end = start + min(volume->sb.blocks-start, volume->sb.blocks_per_group);
  if(!(volume->sb.ro_compat & 1) || num < 2 ||
     powerof(num, 3) || powerof(num, 5) || powerof(num, 7))
    start += 1 + (volume->groups*sizeof(*g)+volume->blocksize-1)/volume->blocksize;
  if(g->blockmap < start || g->inodemap <= g->blockmap ||
     g->table <= g->inodemap || g->table >= end || volume->tablesize > end-g->table)
    return -1;
  return 0;
}

static int
inodeio(uint inum, struct dinode2 *raw, int write)
{
  uint index, group;
  struct group g;

  if(!inum || inum > volume->sb.inodes)
    return -1;
  index = (inum-1) % volume->sb.inodes_per_group;
  group = (inum-1) / volume->sb.inodes_per_group;
  if(groupio(group, &g, 0) < 0)
    return -1;
  return diskio(g.table*volume->sectors, index*volume->inodebytes, raw, sizeof(*raw), write);
}

static int
putsuper(void)
{
  volume->sb.wtime = clockseconds();
  return diskio(2, 0, &volume->sb, sizeof(volume->sb), 1);
}

static int
geometry(void)
{
  struct group g;
  struct dinode2 root;
  uint i;

  if(volume->sb.magic != E2_MAGIC || volume->sb.rev > 1 || volume->sb.log_block > 2 ||
     volume->sb.log_frag != volume->sb.log_block || (volume->sb.compat & E2_JOURNAL) ||
     (volume->sb.incompat & ~E2_FILETYPE) || (volume->sb.ro_compat & ~3U))
    return -1;
  volume->blocksize = 1024U << volume->sb.log_block;
  volume->sectors = volume->blocksize / SECTOR;
  volume->inodebytes = volume->sb.rev ? volume->sb.inode_size : 128;
  volume->firstino = volume->sb.rev ? volume->sb.first_inode : 11;
  if(volume->sb.first_data != (volume->blocksize == 1024) ||
     volume->sb.blocks <= volume->sb.first_data || volume->sb.blocks > volume->capacity/volume->sectors ||
     volume->sb.blocks_per_group < 8 || volume->sb.blocks_per_group > volume->blocksize*8 ||
     volume->sb.frags_per_group != volume->sb.blocks_per_group || !volume->sb.inodes_per_group ||
     volume->sb.inodes_per_group > volume->blocksize*8 || volume->sb.inodes < volume->firstino || volume->firstino < 11 ||
     volume->inodebytes < sizeof(root) || volume->inodebytes > volume->blocksize ||
     (volume->inodebytes & (volume->inodebytes-1)) ||
     volume->sb.free_blocks > volume->sb.blocks || volume->sb.free_inodes > volume->sb.inodes)
    return -1;
  volume->groups = (volume->sb.blocks-volume->sb.first_data-1) / volume->sb.blocks_per_group + 1;
  volume->tablesize = (volume->sb.inodes_per_group*volume->inodebytes + volume->blocksize-1) / volume->blocksize;
  if(volume->groups > 32768 || (volume->sb.inodes-1)/volume->sb.inodes_per_group >= volume->groups ||
     (volume->groups*sizeof(g)+volume->blocksize-1)/volume->blocksize >= volume->sb.blocks - volume->sb.first_data - 1)
    return -1;
  for(i = 0; i < volume->groups; i++)
    if(groupio(i, &g, 0) < 0 ||
       g.free_blocks > min(volume->sb.blocks_per_group, volume->sb.blocks-groupbase(i)) ||
       g.free_inodes > volume->sb.inodes_per_group)
      return -1;
  if(inodeio(EXT2ROOT, &root, 0) < 0 || (root.mode & E2_KIND) != E2_DIR ||
     root.links < 2 || root.size < volume->blocksize || (root.size % volume->blocksize) || root.acl)
    return -1;
  return 0;
}

static int
attach(uint dev)
{
  int result = -1;

  acquiresleep(&lock);
  volume = &disks[dev];
  memset(volume, 0, sizeof(*volume));
  volume->dev = dev;
  volume->capacity = diskprobe(dev);
  if(!volume->capacity){
    result = 0;
  } else if(diskio(2, 0, &volume->sb, sizeof(volume->sb), 0) == 0 && geometry() == 0){
    volume->writable = volume->sb.state == E2_VALID && volume->sb.creator == 0;
    vfs_bind(dev, &ext2_ops);
    result = 1;
  }
  releasesleep(&lock);
  return result;
}

void
ext2init(void)
{
#ifdef P9_LEGACY_ROOT
  initsleeplock(&lock, "ext2");
#else
  struct inode *root, *etc, *shadow = 0;
  struct volume *system = &disks[ROOTDEV];

  initsleeplock(&lock, "ext2");
  fsopsinit();
  if(attach(ROOTDEV) != 1)
    panic("root ext2 unavailable or invalid check the disk image offline");
  if(!system->writable)
    panic("root ext2 needs an offline check run make fsck");

  // protect the account inode by its identity so aliases cannot bypass it
  root = iget(ROOTDEV, ROOTINO);
  ilock(root);
  etc = dirlookup(root, "etc", 0);
  iunlockput(root);
  if(etc){
    ilock(etc);
    if(etc->type == T_DIR && !etc->uid && !(etc->mode & 0022))
      shadow = dirlookup(etc, "shadow", 0);
    iunlockput(etc);
  }
  if(!shadow)
    panic("root ext2 missing trusted /etc/shadow");
  ilock(shadow);
  if(shadow->type != T_FILE || shadow->flags & I_SYMLINK || shadow->uid ||
     shadow->gid || shadow->mode || shadow->nlink != 1 ||
     shadow->size < SECTOR || shadow->size % SECTOR)
    panic("root ext2 invalid /etc/shadow");
  shadowino = shadow->inum;
  iunlockput(shadow);
  cprintf("ext2: / read-write %d byte blocks %d groups%s\n", system->blocksize,
          system->groups, diskislive() ? " in memory" : "");
#endif
}

void
ext2mountdata(void)
{
  struct inode *ip;
  struct p9_direntx entry;
  struct volume *data = &disks[EXT2DEV];
  uint off = 0;
  int n = attach(EXT2DEV);

  if(n <= 0){
    if(n < 0)
      cprintf("ext2: unsupported or invalid secondary disk\n");
    return;
  }
  begin_op();
  ip = nameinofollow("/ext2");
  if(!ip){
    ip = create("/ext2", T_DIR, 0, 0, 0755, 1, 0);
    if(ip)
      iunlock(ip);
  }
  if(ip){
    ilock(ip);
    if(ip->dev == ROOTDEV && ip->type == T_DIR){
      while((n = nextdent(ip, &off, &entry)) > 0)
        if(namecmp(entry.name, ".") && namecmp(entry.name, ".."))
          break;
      if(n == 0)
        mountino = ip->inum;
    }
    iunlockput(ip);
  }
  end_op();
  if(mountino)
    cprintf("ext2: /ext2 %s %d byte blocks %d groups\n",
            data->writable ? "read-write" : "read-only", data->blocksize, data->groups);
  else
    cprintf("ext2: /ext2 must be an empty directory\n");
}

int
ext2sync(void)
{
  uint dev;
  int result = 0;

  acquiresleep(&lock);
  for(dev = ROOTDEV; dev <= INSTALLDEV; dev++){
    if((dev == EXT2DEV && !mountino) || (dev == INSTALLDEV && !installino))
      continue;
    volume = &disks[dev];
    if(volume->dirty && !volume->faulted && !volume->orphans){
      // commit changes before publishing the clean superblock
      if(diskflush(dev) < 0){
        cprintf("ext2: volume %d data flush failed\n", dev);
      } else {
        volume->sb.state |= E2_VALID;
        if(putsuper() == 0 && diskflush(dev) == 0)
          volume->dirty = 0;
        else
          cprintf("ext2: volume %d clean superblock commit failed\n", dev);
      }
      if(volume->dirty){
        volume->sb.state &= ~E2_VALID;
        volume->faulted = 1;
      }
    }
    if(volume->faulted)
      result = -P9_EIO;
    else if(volume->dirty && !result)
      result = -P9_EBUSY;
  }
  releasesleep(&lock);
  return result;
}

uint ext2mountpoint(void) { return mountino; }
uint
ext2host(uint dev)
{
  return dev == EXT2DEV ? mountino : dev == INSTALLDEV ? installino : 0;
}

int
ext2installmount(uint id)
{
  struct inode *ip;
  struct p9_direntx entry;
  uint offset = 0, inum = 0;
  int found = -1;

  if(installino)
    return -P9_EBUSY;
  ip = nameinofollow("/mnt");
  if(!ip)
    return -P9_ENOENT;
  ilock(ip);
  if(ip->dev == ROOTDEV && ip->type == T_DIR && ip->uid == 0 && !(ip->mode & 0022)){
    while((found = nextdent(ip, &offset, &entry)) > 0)
      if(namecmp(entry.name, ".") && namecmp(entry.name, ".."))
        break;
    if(found == 0)
      inum = ip->inum;
  }
  iunlockput(ip);
  if(!inum || diskselect(id) < 0)
    return -P9_EBUSY;
  if(attach(INSTALLDEV) != 1 || !disks[INSTALLDEV].writable){
    vfs_unbind(INSTALLDEV);
    diskdetach();
    return -P9_EIO;
  }
  installino = inum;
  return 0;
}

int
ext2installunmount(void)
{
  if(!installino)
    return -P9_EINVAL;
  if(inodebusy(INSTALLDEV))
    return -P9_EBUSY;
  installino = 0;
  vfs_unbind(INSTALLDEV);
  diskdetach();
  return 0;
}

static int ext2writable(void) { return volume->writable && !volume->faulted; }

static short
type2(ushort mode)
{
  switch(mode & E2_KIND){
  case E2_DIR: return T_DIR;
  case E2_DEV: return T_DEV;
  case E2_SOCK: return T_SOCKET;
  case E2_REG: case E2_LNK: return T_FILE;
  default: return 0;
  }
}

int
ext2load(struct inode *ip)
{
  struct dinode2 raw;
  int result = -1;

  acquiresleep(&lock);
  volume = &disks[ip->dev];
  if(inodeio(ip->inum, &raw, 0) == 0 && !raw.acl && !raw.frag &&
     raw.size <= 0x7fffffffU && (raw.mode & E2_KIND) &&
     ((raw.mode & E2_KIND) != E2_REG || !raw.size_high) &&
     !(raw.osd2[4] | raw.osd2[5] | raw.osd2[6] | raw.osd2[7]) &&
     !(raw.flags & ~(E2_INDEXED | E2_IMMUTABLE | E2_APPEND | 0xffU))){
    ip->type = type2(raw.mode);
    ip->mode = raw.mode & M_VALID;
    ip->uid = raw.uid;
    ip->gid = raw.gid;
    ip->nlink = raw.links;
    ip->size = raw.size;
    ip->mtime = raw.mtime;
    ip->flags = (raw.mode & E2_KIND) == E2_LNK ? I_SYMLINK : 0;
    ip->data_sectors = raw.sectors;
    ip->diskflags = raw.flags;
    ip->major = ip->minor = 0;
    if(ip->type == T_DEV){
      uint rdev = raw.blocks[0] ? raw.blocks[0] : raw.blocks[1];
      uint major = (rdev >> 8) & 0xfff;
      uint minor = (rdev & 255) | ((rdev >> 12) & 0xfff00);

      if(ip->dev != ROOTDEV || major >= NDEV || minor > 32767 || raw.size || raw.sectors)
        ip->type = 0;
      ip->major = major;
      ip->minor = minor;
      memset(ip->addrs, 0, sizeof(ip->addrs));
    } else
      memmove(ip->addrs, raw.blocks, sizeof(raw.blocks));
    if(ip->dev == ROOTDEV && ip->inum == shadowino)
      ip->flags |= I_SHADOW;
    ip->orphan = 0;
    if(ip->type)
      result = 0;
    else
      ip->nlink = 1;
  }
  releasesleep(&lock);
  return result;
}

static int
save(struct inode *ip)
{
  struct dinode2 raw;
  uint kind;

  if(!ext2writable() || inodeio(ip->inum, &raw, 0) < 0)
    return -1;
  kind = ip->type == T_DIR ? E2_DIR : ip->type == T_DEV ? E2_DEV :
         ip->type == T_SOCKET ? E2_SOCK : (ip->flags & I_SYMLINK) ? E2_LNK : E2_REG;
  if(raw.links && !ip->nlink){
    volume->orphans++;
    ip->orphan = 1;
  }
  raw.mode = kind | ip->mode;
  raw.uid = ip->uid;
  raw.gid = ip->gid;
  raw.links = ip->nlink;
  raw.size = ip->size;
  raw.sectors = ip->data_sectors;
  raw.ctime = raw.mtime = clockseconds();
  memmove(raw.blocks, ip->addrs, sizeof(raw.blocks));
  if(ip->type == T_DEV){
    uint minor = ip->minor;
    uint encoded = ((uint)ip->major << 8) | (minor & 255);
    if(minor < 256)
      raw.blocks[0] = encoded;
    else
      raw.blocks[1] = encoded | ((minor & ~255U) << 12);
  }
  int result = inodeio(ip->inum, &raw, 1);
  if(result == 0)
    ip->mtime = raw.mtime;
  return result;
}

int
ext2save(struct inode *ip)
{
  int result;

  acquiresleep(&lock);
  volume = &disks[ip->dev];
  result = save(ip);
  releasesleep(&lock);
  return result;
}

static uint
datastart(struct group *g)
{
  return g->table + volume->tablesize;
}

static int
dataok(uint bn)
{
  struct group g;
  uint gr;

  if(bn < volume->sb.first_data || bn >= volume->sb.blocks)
    return 0;
  gr = (bn-volume->sb.first_data) / volume->sb.blocks_per_group;
  return groupio(gr, &g, 0) == 0 && bn >= datastart(&g);
}

static int
bitmap(uint block, uint bit, int change)
{
  uchar byte, mask = 1 << (bit%8);
  int old;

  if(blockio(block, bit/8, &byte, 1, 0) < 0)
    return -1;
  old = !!(byte & mask);
  if(change && old != (change > 0)){
    byte ^= mask;
    if(blockio(block, bit/8, &byte, 1, 1) < 0)
      return -1;
  }
  return old;
}

static int
bitfind(uint block, uint first, uint end)
{
  uchar data[64];
  uint off, last, bit;

  for(off = first/8; off*8 < end; off += sizeof(data)){
    last = min(sizeof(data), (end+7)/8-off);
    if(blockio(block, off, data, last, 0) < 0)
      break;
    for(bit = (off*8 < first ? first : off*8); bit < min(end, (off+last)*8); bit++)
      if(!(data[bit/8-off] & (1 << (bit%8))))
        return bit;
  }
  return -1;
}

static uint
allocblock(uint hint)
{
  struct group g;
  uint group, scans, base, bn;
  int bit;
  static uchar zero[DISK_BATCH*SECTOR];

  if(!volume->sb.free_blocks)
    return 0;
  for(scans = 0; scans < volume->groups; scans++){
    group = (hint + scans) % volume->groups;
    if(groupio(group, &g, 0) < 0 || !g.free_blocks)
      continue;
    base = groupbase(group);
    bit = bitfind(g.blockmap, datastart(&g)-base, min(volume->sb.blocks_per_group, volume->sb.blocks-base));
    if(bit < 0)
      continue;
    bn = base + bit;
    if(bitmap(g.blockmap, bit, 1) != 0)
      return 0;
    g.free_blocks--;
    volume->sb.free_blocks--;
    if(groupio(group, &g, 1) < 0 || blockio(bn, 0, zero, volume->blocksize, 1) < 0)
      return 0;
    return bn;
  }
  return 0;
}

static int
freeblock(uint bn)
{
  uint group, bit;
  struct group g;

  if(!dataok(bn))
    return -1;
  group = (bn-volume->sb.first_data) / volume->sb.blocks_per_group;
  bit = (bn-volume->sb.first_data) % volume->sb.blocks_per_group;
  if(groupio(group, &g, 0) < 0 || bitmap(g.blockmap, bit, -1) != 1)
    return -1;
  g.free_blocks++;
  volume->sb.free_blocks++;
  return groupio(group, &g, 1);
}

uint
ext2alloc(uint dev, short type)
{
  struct group g;
  struct dinode2 raw;
  uint n, base, first, max, group;
  int bit;

  acquiresleep(&lock);
  volume = &disks[dev];
  if(!ext2writable() || (type != T_FILE && type != T_DIR && type != T_SOCKET && (type != T_DEV || dev != ROOTDEV)) || !volume->sb.free_inodes)
    goto bad;
  for(group = 0; group < volume->groups; group++){
    if(groupio(group, &g, 0) < 0 || !g.free_inodes)
      continue;
    base = group*volume->sb.inodes_per_group;
    if(base >= volume->sb.inodes)
      break;
    max = min(volume->sb.inodes_per_group, volume->sb.inodes-base);
    first = volume->firstino > base + 1 ? volume->firstino - base - 1 : 0;
    bit = bitfind(g.inodemap, first, max);
    if(bit < 0)
      continue;
    n = base+bit+1;
    memset(&raw, 0, sizeof(raw));
    raw.mode = type == T_DIR ? E2_DIR : type == T_DEV ? E2_DEV :
               type == T_SOCKET ? E2_SOCK : E2_REG;
    raw.ctime = raw.mtime = raw.atime = clockseconds();
    if(volume->inodebytes > sizeof(raw)){
      static uchar zero[SECTOR];
      uint off;
      for(off = sizeof(raw); off < volume->inodebytes; off += SECTOR)
        if(diskio(g.table*volume->sectors, bit*volume->inodebytes+off, zero,
                  min(SECTOR, volume->inodebytes-off), 1) < 0)
          goto bad;
    }
    if(bitmap(g.inodemap, bit, 1) != 0)
      goto bad;
    g.free_inodes--;
    volume->sb.free_inodes--;
    if(type == T_DIR)
      g.used_dirs++;
    if(inodeio(n, &raw, 1) < 0 || groupio(group, &g, 1) < 0)
      goto bad;
    releasesleep(&lock);
    return n;
  }
bad:
  releasesleep(&lock);
  return 0;
}

// a hole is zero and an invalid block chain is an error
static int
mapblock(struct inode *ip, uint index, int alloc, uint *result)
{
  uint pointers = volume->blocksize/sizeof(uint), span, depth, slot, i, off, prev;
  uint bn, group, next, created[4], ncreated = 0;

  group = (ip->inum-1) / volume->sb.inodes_per_group;
  if(index < E2_DIRECT){
    slot = index;
    depth = 0;
    span = 1;
  } else {
    index -= E2_DIRECT;
    for(depth = 1, span = pointers; depth < 3 && index >= span; depth++, span *= pointers)
      index -= span;
    if(index >= span)
      return -1;
    slot = E2_DIRECT + depth - 1;
  }

  bn = ip->addrs[slot];
  prev = 0;
  off = 0;
  for(i = 0; ; i++){
    if(!bn){
      if(!alloc){
        *result = 0;
        return 0;
      }
      // build missing branches first so enospc leaves the inode unchanged
      for(; ncreated <= depth-i; ncreated++){
        created[ncreated] = allocblock(group);
        if(!created[ncreated])
          goto rollback;
      }
      next = created[0];
      if(!prev)
        ip->addrs[slot] = next;
      for(uint j = 1; j < ncreated; j++){
        uint sub = span / pointers;
        uint entry = (index / sub) * sizeof(uint);
        index %= sub;
        span = sub;
        if(blockio(created[j-1], entry, &created[j], sizeof(uint), 1) < 0)
          return -1;
      }
      if(prev && blockio(prev, off, &next, sizeof(next), 1) < 0)
        return -1;
      ip->data_sectors += volume->sectors*ncreated;
      *result = created[ncreated-1];
      return 0;
    }
    if(!dataok(bn))
      return corrupt("block pointer", ip->inum);
    if(i == depth){
      *result = bn;
      return 0;
    }
    span /= pointers;
    off = (index / span) * sizeof(uint);
    index %= span;
    prev = bn;
    if(blockio(prev, off, &bn, sizeof(bn), 0) < 0)
      return -1;
  }

rollback:
  while(ncreated)
    freeblock(created[--ncreated]);
  return -1;
}

static int
readfile(struct inode *ip, char *dst, uint off, uint n)
{
  uint done, bytes, bn, inside;

  if(off >= ip->size)
    return 0;
  n = min(n, ip->size-off);
  if((ip->flags & I_SYMLINK) && !ip->data_sectors){
    if(ip->size > sizeof(ip->addrs))
      return -1;
    memmove(dst, (char*)ip->addrs+off, n);
    return n;
  }
  for(done = 0; done < n; done += bytes, off += bytes){
    inside = off % volume->blocksize;
    bytes = min(n-done, volume->blocksize-inside);
    if(mapblock(ip, off/volume->blocksize, 0, &bn) < 0)
      return done ? (int)done : -1;
    if(!bn)
      memset(dst+done, 0, bytes);
    else if(blockio(bn, inside, dst+done, bytes, 0) < 0)
      return done ? (int)done : -1;
  }
  return n;
}

int
ext2read(struct inode *ip, char *dst, uint off, uint n)
{
  int result;

  acquiresleep(&lock);
  volume = &disks[ip->dev];
  result = readfile(ip, dst, off, n);
  releasesleep(&lock);
  return result;
}

static int
canwrite(struct inode *ip)
{
  return ext2writable() && !(ip->diskflags & (E2_IMMUTABLE | E2_APPEND | E2_INDEXED));
}

static int
writefile(struct inode *ip, const char *src, uint off, uint n)
{
  uint done, bytes, bn, inside;

  if(!canwrite(ip) || off > 0x7fffffffU || n > 0x7fffffffU-off)
    return -1;
  if(!n)
    return 0;
  if((ip->flags & I_SYMLINK) && !ip->data_sectors && !off && n <= sizeof(ip->addrs)){
    memmove(ip->addrs, src, n);
    ip->size = n;
    return save(ip) == 0 ? (int)n : -1;
  }
  for(done = 0; done < n; done += bytes, off += bytes){
    inside = off % volume->blocksize;
    bytes = min(n-done, volume->blocksize-inside);
    if(mapblock(ip, off/volume->blocksize, 1, &bn) < 0 ||
       blockio(bn, inside, (void*)(src+done), bytes, 1) < 0)
      break;
  }
  if(done && off > ip->size)
    ip->size = off;
  if(done && save(ip) < 0)
    return -1;
  return done || !n ? (int)done : -1;
}

int
ext2write(struct inode *ip, const char *src, uint off, uint n)
{
  int result;

  acquiresleep(&lock);
  volume = &disks[ip->dev];
  result = writefile(ip, src, off, n);
  releasesleep(&lock);
  return result;
}

int
ext2maywrite(struct inode *ip)
{
  int result;

  acquiresleep(&lock);
  volume = &disks[ip->dev];
  result = canwrite(ip);
  releasesleep(&lock);
  return result;
}

static int
cleartree(uint bn, uint level, uint *budget)
{
  uint offset, ptr;

  if(!bn)
    return 0;
  if(!*budget || !dataok(bn))
    return -1;
  (*budget)--;
  if(level)
    for(offset = 0; offset < volume->blocksize; offset += sizeof(ptr))
      if(blockio(bn, offset, &ptr, sizeof(ptr), 0) < 0 ||
         cleartree(ptr, level-1, budget) < 0)
        return -1;
  return freeblock(bn);
}

static int
truncate(struct inode *ip)
{
  uint addrs[15], i, blocks = ip->data_sectors / volume->sectors;
  int inline_link = (ip->flags & I_SYMLINK) && !ip->data_sectors;

  if(!canwrite(ip))
    return -1;
  memmove(addrs, ip->addrs, sizeof(addrs));
  memset(ip->addrs, 0, sizeof(ip->addrs));
  ip->size = ip->data_sectors = 0;
  if(save(ip) < 0)
    return -1;
  if(inline_link)
    return 0;
  for(i = 0; i < NELEM(addrs); i++)
    if(cleartree(addrs[i], i < E2_DIRECT ? 0 : i-E2_DIRECT+1, &blocks) < 0){
      volume->faulted = 1;
      return -1;
    }
  return 0;
}

int
ext2truncate(struct inode *ip)
{
  int result;

  acquiresleep(&lock);
  volume = &disks[ip->dev];
  result = truncate(ip);
  releasesleep(&lock);
  return result;
}

void
ext2free(struct inode *ip)
{
  struct group g;
  struct dinode2 raw;
  uint group, bit;

  acquiresleep(&lock);
  volume = &disks[ip->dev];
  if(ip->inum < volume->firstino || truncate(ip) < 0)
    goto out;
  bit = (ip->inum-1) % volume->sb.inodes_per_group;
  group = (ip->inum-1) / volume->sb.inodes_per_group;
  memset(&raw, 0, sizeof(raw));
  raw.dtime = clockseconds();
  if(inodeio(ip->inum, &raw, 1) < 0 || groupio(group, &g, 0) < 0 ||
     bitmap(g.inodemap, bit, -1) != 1)
    goto out;
  g.free_inodes++;
  volume->sb.free_inodes++;
  if(ip->type == T_DIR)
    g.used_dirs--;
  if(groupio(group, &g, 1) == 0 && ip->orphan){
    volume->orphans--;
    ip->orphan = 0;
  }
out:
  releasesleep(&lock);
}

static uint e2align(uint n) { return (n+3) & ~3U; }

static int
getdent(struct inode *dp, uint off, struct dent *de)
{
  uint inside = off % volume->blocksize;

  if((off & 3) || off >= dp->size)
    return -1;
  if(dp->size-off < sizeof(*de) || volume->blocksize-inside < sizeof(*de) ||
     readfile(dp, (char*)de, off, sizeof(*de)) != sizeof(*de))
    return corrupt("directory header", dp->inum);
  if(de->type > ((volume->sb.incompat & E2_FILETYPE) ? 7 : 0))
    return corrupt("directory type", dp->inum);
  if(de->len < sizeof(*de) || (de->len & 3) ||
     de->len > dp->size-off || de->len > volume->blocksize-inside ||
     e2align(sizeof(*de)+de->name_len) > de->len || de->inum > volume->sb.inodes ||
     (de->inum && !de->name_len))
    return corrupt("directory entry", dp->inum);
  return 0;
}

static int
nextdent2(struct inode *dp, uint *off, struct p9_direntx *entry)
{
  struct dent de;
  uint pos, i;

  while(*off < dp->size){
    pos = *off;
    if(getdent(dp, pos, &de) < 0)
      return -1;
    *off += de.len;
    if(!de.inum)
      continue;
    if(readfile(dp, entry->name, pos+sizeof(de), de.name_len) != de.name_len)
      return -1;
    for(i = 0; i < de.name_len; i++)
      if(!entry->name[i] || entry->name[i] == '/')
        return corrupt("directory name", dp->inum);
    entry->name[de.name_len] = 0;
    entry->inum = de.inum;
    return 1;
  }
  return 0;
}

int
ext2next(struct inode *dp, uint *off, struct p9_direntx *entry)
{
  int result;

  acquiresleep(&lock);
  volume = &disks[dp->dev];
  result = nextdent2(dp, off, entry);
  releasesleep(&lock);
  return result;
}

int
ext2lookup(struct inode *dp, const char *name, uint *inum, uint *foundoff)
{
  uint pos = 0, current;
  struct p9_direntx entry;
  struct dent de;
  int result = 0;

  acquiresleep(&lock);
  volume = &disks[dp->dev];
  while(pos < dp->size){
    current = pos;
    if(getdent(dp, pos, &de) < 0){
      result = -1;
      break;
    }
    pos += de.len;
    if(de.inum && de.name_len == strlen(name)){
      if(readfile(dp, entry.name, current+sizeof(de), de.name_len) != de.name_len){
        result = -1;
        break;
      }
      entry.name[de.name_len] = 0;
      if(!strncmp(name, entry.name, P9_NAME_MAX+1)){
        *inum = de.inum;
        if(foundoff)
          *foundoff = current;
        result = 1;
        break;
      }
    }
  }
  releasesleep(&lock);
  return result;
}

static int
putdent(uint bn, uint inside, const char *name, uint inum, uint room)
{
  struct {
    struct dent header;
    char name[P9_NAME_MAX+1];
  } record;
  struct dinode2 raw;
  uint n = name ? strlen(name) : 0;

  if(inside > volume->blocksize || room > volume->blocksize-inside ||
     n > P9_NAME_MAX || e2align(sizeof(struct dent)+n) > room)
    return -1;
  record.header = (struct dent){ inum, room, n, 0 };
  if(inum && (volume->sb.incompat & E2_FILETYPE)){
    if(inodeio(inum, &raw, 0) < 0)
      return -1;
    switch(raw.mode & E2_KIND){
    case E2_REG: record.header.type = 1; break;
    case E2_DIR: record.header.type = 2; break;
    case E2_DEV: record.header.type = 3; break;
    case E2_LNK: record.header.type = 7; break;
    case E2_SOCK: record.header.type = 6; break;
    }
  }
  if(n)
    memmove(record.name, name, n);
  return blockio(bn, inside, &record, sizeof(struct dent)+n, 1);
}

static int
insert(struct inode *dp, const char *name, uint inum)
{
  struct dent de;
  uint off, used, bn, inside, needed = e2align(sizeof(de)+strlen(name));
  ushort newlen;

  if(!canwrite(dp) || strlen(name) > P9_NAME_MAX || (dp->size % volume->blocksize) || needed > volume->blocksize)
    return -1;
  for(off = 0; off < dp->size; off += de.len){
    if(getdent(dp, off, &de) < 0)
      return -1;
    used = de.inum ? e2align(sizeof(de)+de.name_len) : 0;
    if(de.len - used < needed)
      continue;
    if(mapblock(dp, off/volume->blocksize, 0, &bn) < 0 || !bn)
      return -1;
    inside = off % volume->blocksize;
    if(putdent(bn, inside+used, name, inum, de.len-used) < 0)
      return -1;
    if(used){
      newlen = used;
      if(blockio(bn, inside+sizeof(uint), &newlen, sizeof(newlen), 1) < 0)
        return -1;
    }
    return 0;
  }

  if(dp->size > 0x7fffffffU-volume->blocksize ||
     mapblock(dp, off/volume->blocksize, 1, &bn) < 0 ||
     putdent(bn, 0, name, inum, volume->blocksize) < 0)
    return -1;
  dp->size += volume->blocksize;
  return save(dp);
}

int
ext2insert(struct inode *dp, const char *name, uint inum)
{
  int result;

  acquiresleep(&lock);
  volume = &disks[dp->dev];
  result = insert(dp, name, inum);
  releasesleep(&lock);
  return result;
}

int
ext2setentry(struct inode *dp, uint off, const char *name, uint inum)
{
  struct dent de;
  uint bn;
  int result = -1;

  acquiresleep(&lock);
  volume = &disks[dp->dev];
  if(canwrite(dp) && getdent(dp, off, &de) == 0 &&
     mapblock(dp, off/volume->blocksize, 0, &bn) == 0 && bn)
    result = putdent(bn, off % volume->blocksize, name, inum, de.len);
  releasesleep(&lock);
  return result;
}
