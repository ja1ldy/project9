/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

/* convert a v0 1 disk in memory then write a separate v0 2 image */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "installpath.h"

#define stat xv6_stat
#include "types.h"
#include "../include/legacyfs.h"
#include "stat.h"
#include "mode.h"
#include "param.h"

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error fsupgrade requires a little-endian host
#endif

struct old_inode {
  ushort type, major, minor, nlink;
  uint size;
  uint addrs[NDIRECT + 1];
};

enum { OLD_OWNED = 1, OLD_PRIVATE = 2, OLD_SHADOW = 4 };
static unsigned char image[FSSIZE * BSIZE], seen[FSSIZE];
static struct superblock sb;
static struct dinode *inodes;
static uint first_data;

static void
fail(const char *message)
{
  fprintf(stderr, "fsupgrade: %s\n", message);
  exit(1);
}

static void *
sector(uint block)
{
  if(block >= FSSIZE)
    fail("block number is out of range");
  return image + block * BSIZE;
}

static unsigned char *
bitmap(uint block)
{
  return (unsigned char*)sector(BBLOCK(block, sb)) + block % BPB / 8;
}

static int
used(uint block)
{
  return (*bitmap(block) & (1U << (block % 8))) != 0;
}

static void
mark(uint block, int value)
{
  if(value)
    *bitmap(block) |= 1U << (block % 8);
  else
    *bitmap(block) &= ~(1U << (block % 8));
}

static void
checkblock(uint block)
{
  if(block < first_data || block >= sb.size || !used(block) || seen[block])
    fail("invalid or shared data block");
  seen[block] = 1;
}

static uint
allocblock(void)
{
  uint b;

  for(b = first_data; b < sb.size; b++){
    if(!used(b)){
      mark(b, 1);
      memset(sector(b), 0, BSIZE);
      return b;
    }
  }
  fail("not enough free blocks to install the updated programs");
  return 0;
}

static struct dinode *
inode(uint number)
{
  if(!number || number >= sb.ninodes)
    fail("invalid inode number");
  return &inodes[number];
}

static int
type(const struct dinode *ip)
{
  return (ip->kind & INODE_TYPE_MASK) >> INODE_TYPE_SHIFT;
}

static uint *
blockslot(struct dinode *ip, uint logical, int allocate)
{
  uint *indirect;

  if(logical < NDIRECT)
    return &ip->addrs[logical];
  if(logical >= MAXFILE)
    fail("file exceeds the xv6 file size limit");
  if(!ip->addrs[NDIRECT]){
    if(!allocate)
      return NULL;
    ip->addrs[NDIRECT] = allocblock();
  }
  indirect = sector(ip->addrs[NDIRECT]);
  return &indirect[logical - NDIRECT];
}

static struct dirent *
directory_slot(struct dinode *dir, uint offset, int allocate)
{
  uint *block = blockslot(dir, offset / BSIZE, allocate);

  if(!block || (!*block && !allocate))
    fail("directory contains a gap");
  if(!*block)
    *block = allocblock();
  return (struct dirent*)((char*)sector(*block) + offset % BSIZE);
}

static struct dirent *
findentry(struct dinode *dir, const char *name, int allocate)
{
  struct dirent *entry, *empty = NULL;
  uint offset;

  if(type(dir) != T_DIR || dir->size % sizeof(*entry))
    fail("invalid directory");
  for(offset = 0; offset < dir->size; offset += sizeof(*entry)){
    entry = directory_slot(dir, offset, 0);
    if(entry->inum && !strncmp(entry->name, name, DIRSIZ))
      return entry;
    if(!entry->inum && !empty)
      empty = entry;
  }
  if(!allocate || empty)
    return allocate ? empty : NULL;
  entry = directory_slot(dir, offset, 1);
  dir->size += sizeof(*entry);
  return entry;
}

static struct dinode *
child(struct dinode *dir, const char *name)
{
  struct dirent *entry = findentry(dir, name, 0);
  return entry ? inode(entry->inum) : NULL;
}

static void
convert(void)
{
  uint number, logical, blocks;
  struct old_inode old;
  struct dinode *ip;
  uint *slot;
  int mode, flags;

  for(number = 1; number < sb.ninodes; number++){
    ip = inode(number);
    memcpy(&old, ip, sizeof(old));
    if(!old.type)
      continue;
    if(old.type < T_DIR || old.type > T_DEV || old.size > MAXFILE * BSIZE)
      fail("invalid old inode");
    memset(ip, 0, sizeof(*ip));
    ip->size = old.size;
    ip->nlink = old.nlink;
    flags = 0;
    if(old.type == T_DEV){
      mode = old.major == 1 ? 0666 : 0600;
      ip->size = 0;
      ip->addrs[0] = old.major;
      ip->addrs[1] = old.minor;
    } else {
      if(old.minor & OLD_OWNED)
        ip->uid = ip->gid = old.major;
      mode = old.minor & OLD_OWNED ? 0644 : 0666;
      if(old.minor & OLD_PRIVATE)
        mode = 0600;
      if(old.type == T_DIR)
        mode |= (mode & 0444) >> 2;
      if(old.minor & OLD_SHADOW){
        mode = 0;
        flags = I_SHADOW;
      }
      memcpy(ip->addrs, old.addrs, sizeof(ip->addrs));
      if(ip->addrs[NDIRECT])
        checkblock(ip->addrs[NDIRECT]);
      blocks = (ip->size + BSIZE - 1) / BSIZE;
      for(logical = 0; logical < MAXFILE; logical++){
        slot = blockslot(ip, logical, 0);
        if(slot && *slot)
          checkblock(*slot);
        else if(logical < blocks)
          fail("file is missing a data block");
      }
      if(old.type == T_FILE && ip->size >= 4 &&
         !memcmp(sector(ip->addrs[0]), "\177ELF", 4))
        mode |= (mode & 0444) >> 2;
    }
    ip->kind = (old.type << INODE_TYPE_SHIFT) | flags | mode;
  }
}

static uint
vacant_inode(void)
{
  uint number;
  for(number = 1; number < sb.ninodes; number++)
    if(!inode(number)->kind)
      return number;
  fail("not enough free inodes to install the updated programs");
  return 0;
}

static void
release_blocks(struct dinode *ip)
{
  uint logical, *slot;

  for(logical = 0; logical < MAXFILE; logical++){
    slot = blockslot(ip, logical, 0);
    if(slot && *slot)
      mark(*slot, 0);
  }
  if(ip->addrs[NDIRECT])
    mark(ip->addrs[NDIRECT], 0);
}

static void
install(struct dinode *dir, const char *filename, int mode)
{
  struct dirent *entry;
  struct dinode *ip;
  uint number, *slot;
  const char *name = strrchr(filename, '/');
  FILE *file;
  size_t count;
  unsigned char buf[BSIZE];

  name = name ? name + 1 : filename;
  if(!*name || *name == '.' || strlen(name) > DIRSIZ)
    fail("invalid installed file name");
  if(!(file = fopen(filename, "rb"))){
    perror(filename);
    exit(1);
  }
  entry = findentry(dir, name, 1);
  number = entry->inum;
  if(number){
    ip = inode(number);
    if(type(ip) != T_FILE || !ip->nlink || (ip->kind & (I_SHADOW | I_SYMLINK)))
      fail("cannot replace an unexpected inode");
    if(ip->nlink == 1){
      release_blocks(ip);
    } else {
      ip->nlink--;
      number = vacant_inode();
    }
  } else {
    number = vacant_inode();
  }
  ip = inode(number);
  memset(ip, 0, sizeof(*ip));
  ip->kind = T_FILE << INODE_TYPE_SHIFT | mode;
  ip->nlink = 1;
  while((count = fread(buf, 1, BSIZE, file)) > 0){
    slot = blockslot(ip, ip->size / BSIZE, 1);
    *slot = allocblock();
    memcpy(sector(*slot), buf, count);
    ip->size += count;
  }
  if(ferror(file))
    fail("cannot read an install file");
  fclose(file);
  entry->inum = number;
  memset(entry->name, 0, sizeof(entry->name));
  memcpy(entry->name, name, strlen(name));
}

static struct dinode *
system_dir(struct dinode *parent, const char *name, ushort mode)
{
  struct dinode *ip = child(parent, name);
  if(!ip || type(ip) != T_DIR || ip->uid)
    fail("a system directory is missing or has unexpected ownership");
  ip->kind = T_DIR << INODE_TYPE_SHIFT | mode;
  ip->gid = 0;
  return ip;
}

// keep existing metadata when installing into a current disk
static struct dinode *
trusted_dir(struct dinode *parent, const char *name)
{
  struct dinode *ip = child(parent, name);

  if(!ip || type(ip) != T_DIR || ip->uid || (ip->kind & 0022))
    fail("a system directory is missing or writable by ordinary users");
  return ip;
}

static struct dinode *
manual_dir(struct dinode *share)
{
  struct dirent *entry = findentry(share, "man", 0);
  struct dinode *dir;
  uint inum;

  if(entry)
    return trusted_dir(share, "man");
  inum = vacant_inode();
  dir = inode(inum);
  memset(dir, 0, sizeof(*dir));
  dir->kind = T_DIR << INODE_TYPE_SHIFT | 0755;
  dir->nlink = 1;
  entry = findentry(dir, ".", 1);
  entry->inum = inum;
  memcpy(entry->name, ".", 2);
  entry = findentry(dir, "..", 1);
  entry->inum = share - inodes;
  memcpy(entry->name, "..", 3);
  entry = findentry(share, "man", 1);
  entry->inum = inum;
  memset(entry->name, 0, sizeof(entry->name));
  memcpy(entry->name, "man", 3);
  share->nlink++;
  return dir;
}

static void
validate_files(void)
{
  uint inum, logical, blocks, *slot;
  struct dinode *ip;

  for(inum = 1; inum < sb.ninodes; inum++){
    ip = inode(inum);
    if(!ip->kind || type(ip) == T_DEV)
      continue;
    if((type(ip) != T_FILE && type(ip) != T_DIR) || ip->size > MAXFILE * BSIZE)
      fail("invalid inode");
    if(ip->addrs[NDIRECT])
      checkblock(ip->addrs[NDIRECT]);
    blocks = (ip->size + BSIZE - 1) / BSIZE;
    for(logical = 0; logical < MAXFILE; logical++){
      slot = blockslot(ip, logical, 0);
      if(slot && *slot)
        checkblock(*slot);
      else if(logical < blocks)
        fail("file is missing a data block");
    }
  }
}

static int
load(int fd, int inspect, int refresh)
{
  size_t copied = 0;
  ssize_t n;
  unsigned char extra;

  while(copied < sizeof(image)){
    n = read(fd, image + copied, sizeof(image) - copied);
    if(n <= 0)
      fail("cannot read a complete disk image");
    copied += n;
  }
  if(read(fd, &extra, 1) != 0)
    fail("unexpected image size");
  memcpy(&sb, sector(1), sizeof(sb));
  if((sb.magic != 0 && sb.magic != FS_MAGIC) ||
     sb.size != FSSIZE || sb.ninodes < 8 ||
     sb.ninodes > NINODE || sb.nblocks >= sb.size)
    fail("unexpected Project9 disk format");
  first_data = sb.size - sb.nblocks;
  if(sb.logstart < 2 || sb.logstart >= sb.inodestart ||
     sb.nlog != sb.inodestart - sb.logstart ||
     sb.inodestart >= sb.bmapstart || sb.bmapstart >= first_data ||
     sb.bmapstart - sb.inodestart < (sb.ninodes + IPB - 1) / IPB)
    fail("invalid filesystem layout");
  if(inspect)
    return sb.magic == FS_MAGIC ? 2 : 1;
  if(sb.magic == FS_MAGIC && !refresh)
    fail("the input already uses the v0.2 disk format");
  if(sb.magic != FS_MAGIC && refresh)
    fail("use a previous Project9 release to upgrade a v0.1 disk");
  if(*(uint*)sector(sb.logstart) != 0)
    fail("the journal is pending, boot the old kernel and shut it down first");
  inodes = sector(sb.inodestart);
  return 1;
}

int
main(int argc, char **argv)
{
  struct flock lock = { .l_type = F_RDLCK, .l_whence = SEEK_SET };
  struct dinode *root, *bin, *etc, *share, *man, *shadow;
  size_t copied = 0;
  int i, source, output, inspect, refresh, version;
  const char *input;
  ssize_t n;

  _Static_assert(sizeof(struct dinode) == 64, "unexpected v0.2 inode size");
  _Static_assert(sizeof(struct old_inode) == 64, "unexpected v0.1 inode size");
  if(argc < 3)
    fail("usage: fsupgrade [--refresh] old.img new.img [file ...] | fsupgrade --disk-version image");
  refresh = !strcmp(argv[1], "--refresh");
  if(refresh){
    argc--;
    argv++;
    if(argc < 3)
      fail("usage: fsupgrade --refresh old.img new.img [file ...]");
  }
  inspect = !strcmp(argv[1], "--disk-version");
  if(inspect && argc != 3)
    fail("usage: fsupgrade --disk-version image");
  input = inspect ? argv[2] : argv[1];
  if((source = open(input, O_RDONLY)) < 0){
    perror(input);
    return 1;
  }
  if(fcntl(source, F_SETLK, &lock) < 0)
    fail("the source disk is in use, quit QEMU before upgrading");
  version = load(source, inspect, refresh);
  if(inspect){
    printf("%d\n", version);
    close(source);
    return 0;
  }
  if(refresh)
    validate_files();
  else
    convert();

  root = inode(LEGACY_ROOTINO);
  if(type(root) != T_DIR || root->uid || (refresh && (root->kind & 0022)))
    fail("invalid root directory");
  if(refresh){
    bin = trusted_dir(root, "bin");
    etc = trusted_dir(root, "etc");
    share = trusted_dir(trusted_dir(root, "usr"), "share");
  } else {
    root->kind = T_DIR << INODE_TYPE_SHIFT | 0755;
    bin = system_dir(root, "bin", 0755);
    etc = system_dir(root, "etc", 0755);
    system_dir(root, "dev", 0755);
    system_dir(root, "home", 0755);
    system_dir(root, "root", 0700);
    system_dir(root, "tmp", 01777);
    share = system_dir(system_dir(root, "usr", 0755), "share", 0755);
    system_dir(root, "var", 0755);
  }
  shadow = child(etc, "shadow");
  if(!shadow || type(shadow) != T_FILE || !(shadow->kind & I_SHADOW))
    fail("missing protected account database");
  man = manual_dir(share);
  for(i = 3; i < argc; i++){
    if(is_manual_path(argv[i]))
      install(man, argv[i], 0644);
    else
      install(bin, argv[i], 0755);
  }

  sb.magic = FS_MAGIC;
  memcpy(sector(1), &sb, sizeof(sb));
  if((output = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0600)) < 0){
    perror(argv[2]);
    return 1;
  }
  while(copied < sizeof(image)){
    n = write(output, image + copied, sizeof(image) - copied);
    if(n <= 0)
      break;
    copied += n;
  }
  if(copied != sizeof(image) || fsync(output) < 0){
    close(output);
    unlink(argv[2]);
    fail("could not write the new image");
  }
  close(output);
  close(source);
  return 0;
}
