/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "types.h"
#include "../include/legacyfs.h"
#include "mode.h"
#include "param.h"
#include "fscheck.h"

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error fsck requires a little endian host
#endif

static uchar image[FSSIZE * BSIZE];
static struct superblock sb;
static struct dinode inodes[NINODE];
static int owner[FSSIZE];
static uint refs[NINODE], subdirs[NINODE], parent[NINODE], dotdot[NINODE];
static int errors;

static void
report(const char *fmt, ...)
{
  va_list args;

  if(errors++ >= 60)
    return;
  fputs("fsck: ", stderr);
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputc('\n', stderr);
}

static int
read_image(const char *path)
{
  struct flock lock = { .l_type = F_RDLCK, .l_whence = SEEK_SET };
  struct stat st;
  size_t done = 0;
  ssize_t n;
  int fd = open(path, O_RDONLY);

  if(fd < 0){
    perror(path);
    return -1;
  }
  if(fcntl(fd, F_SETLK, &lock) < 0){
    fprintf(stderr, "fsck: cannot lock %s for offline inspection\n", path);
    close(fd);
    return -1;
  }
  if(fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size != sizeof(image)){
    fprintf(stderr, "fsck: expected a %zu byte Project9 image\n", sizeof(image));
    close(fd);
    return -1;
  }
  while(done < sizeof(image)){
    n = read(fd, image+done, sizeof(image)-done);
    if(n < 0 && errno == EINTR)
      continue;
    if(n <= 0){
      fprintf(stderr, "fsck: incomplete read of %s\n", path);
      close(fd);
      return -1;
    }
    done += n;
  }
  close(fd);
  return 0;
}

static uchar *
block(uint number)
{
  return image + number * BSIZE;
}

static int
journal(void)
{
  int count, i, j;
  uint targets[LOGSIZE];
  uchar *header = block(sb.logstart);

  memcpy(&count, header, sizeof(count));
  if(count < 0 || count > LOGSIZE || (uint)count >= sb.nlog){
    report("invalid journal count %d", count);
    return -1;
  }
  memcpy(targets, header + sizeof(count), count * sizeof(*targets));
  for(i = 0; i < count; i++){
    if(!journalentry(&sb, targets[i])){
      report("journal target %u is outside the writable filesystem", targets[i]);
      return -1;
    }
    for(j = 0; j < i; j++)
      if(targets[j] == targets[i]){
        report("journal repeats block %u", targets[i]);
        return -1;
      }
  }

  // check the filesystem view that the kernel will recover
  for(i = 0; i < count; i++)
    memcpy(block(targets[i]), block(sb.logstart+i+1), BSIZE);
  if(count)
    printf("fsck: checked with %d committed journal blocks applied in memory\n", count);
  return 0;
}

static uint
type(uint inum)
{
  return (inodes[inum].kind & INODE_TYPE_MASK) >> INODE_TYPE_SHIFT;
}

static int
data_block(uint n)
{
  return n >= sb.size - sb.nblocks && n < sb.size;
}

static int
claim(uint n, uint inum)
{
  if(!data_block(n)){
    report("inode %u has invalid data block %u", inum, n);
    return 0;
  }
  if(owner[n]){
    report("block %u is shared by inode %d and inode %u", n, owner[n], inum);
    return 0;
  }
  owner[n] = inum;
  return 1;
}

static void
check_pointer(uint n, uint inum, uint index, uint needed)
{
  if(index < needed && !n)
    report("inode %u is missing file block %u", inum, index);
  if(index >= needed && n)
    report("inode %u has a block past its end at index %u", inum, index);
  if(n)
    claim(n, inum);
}

static void
check_inode(uint inum)
{
  const struct dinode *ip = &inodes[inum];
  uint i, needed, ptr, indirect = ip->addrs[NDIRECT];
  uint kind = type(inum);

  if(!ip->kind)
    return;
  if(!kind || (ip->kind & ~(INODE_TYPE_MASK | M_VALID | I_SHADOW | I_SYMLINK))){
    report("inode %u has invalid type or flags", inum);
    return;
  }
  if((ip->kind & I_SYMLINK) &&
     (kind != 2 || !ip->size || ip->size >= P9_PATH_MAX || (ip->kind & I_SHADOW)))
    report("inode %u has invalid symbolic link metadata", inum);
  if(kind == 3){
    if(ip->size)
      report("device inode %u has a nonzero size", inum);
    for(i = 2; i <= NDIRECT; i++)
      if(ip->addrs[i])
        report("device inode %u has a data pointer", inum);
    return;
  }
  if(ip->size > MAXFILE * BSIZE){
    report("inode %u exceeds the file size limit", inum);
    return;
  }
  if(kind == 1 && ip->size % sizeof(struct dirent))
    report("directory inode %u ends with a partial entry", inum);
  needed = (ip->size + BSIZE-1) / BSIZE;
  for(i = 0; i < NDIRECT; i++)
    check_pointer(ip->addrs[i], inum, i, needed);
  if(!indirect && needed > NDIRECT)
    report("inode %u is missing its indirect block", inum);
  if(indirect){
    if(needed <= NDIRECT)
      report("inode %u has an unused indirect block", inum);
    if(claim(indirect, inum))
      for(i = 0; i < NINDIRECT; i++){
        memcpy(&ptr, block(indirect) + i*sizeof(ptr), sizeof(ptr));
        check_pointer(ptr, inum, NDIRECT+i, needed);
      }
  }
}

static int
direntry(uint inum, uint off, struct dirent *entry)
{
  const struct dinode *ip = &inodes[inum];
  uint index = off / BSIZE, n, indirect = ip->addrs[NDIRECT];

  if(index < NDIRECT)
    n = ip->addrs[index];
  else if(index < MAXFILE && data_block(indirect))
    memcpy(&n, block(indirect) + (index-NDIRECT)*sizeof(n), sizeof(n));
  else
    return 0;
  if(!data_block(n) || owner[n] != (int)inum)
    return 0;
  memcpy(entry, block(n) + off % BSIZE, sizeof(*entry));
  return 1;
}

static int
by_name(const void *a, const void *b)
{
  const struct dirent *first = a, *second = b;

  return strncmp(first->name, second->name, DIRSIZ);
}

static void
check_directory(uint inum)
{
  static struct dirent names[MAXFILE * BSIZE / sizeof(struct dirent)];
  struct dirent de;
  uint off, n, i, size = inodes[inum].size, count = 0, dots = 0, parents = 0;

  if(size > MAXFILE * BSIZE)
    return;
  for(off = 0; off + sizeof(de) <= size; off += sizeof(de)){
    if(!direntry(inum, off, &de)){
      report("directory inode %u cannot read entry at byte %u", inum, off);
      break;
    }
    n = de.inum;
    if(!n)
      continue;
    names[count++] = de;
    if(n >= sb.ninodes || !type(n)){
      report("directory inode %u references missing inode %u", inum, n);
      continue;
    }
    for(i = 0; i < DIRSIZ && de.name[i] && de.name[i] != '/'; i++)
      ;
    if(!i || (i < DIRSIZ && de.name[i] == '/')){
      report("directory inode %u has an invalid name", inum);
      continue;
    }
    if(!strncmp(de.name, ".", DIRSIZ)){
      dots++;
      if(n != inum)
        report("directory inode %u has an incorrect dot entry", inum);
    } else if(!strncmp(de.name, "..", DIRSIZ)){
      parents++;
      dotdot[inum] = n;
      if(type(n) != 1)
        report("directory inode %u has a non-directory parent", inum);
    } else {
      refs[n]++;
      if(type(n) == 1){
        subdirs[inum]++;
        if(n == LEGACY_ROOTINO || parent[n])
          report("directory inode %u has more than one parent link", n);
        else
          parent[n] = inum;
      }
    }
  }
  if(dots != 1 || parents != 1)
    report("directory inode %u needs one dot and one dot-dot entry", inum);
  qsort(names, count, sizeof(*names), by_name);
  for(i = 1; i < count; i++)
    if(!by_name(&names[i-1], &names[i]))
      report("directory inode %u has duplicate name %.14s", inum, names[i].name);
}

static void
check_links(uint inum)
{
  uint n, hops, expected = refs[inum];

  if(!type(inum))
    return;
  if(type(inum) == 1){
    expected = 1 + subdirs[inum];
    n = inum == LEGACY_ROOTINO ? LEGACY_ROOTINO : parent[inum];
    if(dotdot[inum] != n)
      report("directory inode %u has a mismatched dot-dot entry", inum);
    for(hops = 0; hops < sb.ninodes && n && n != LEGACY_ROOTINO; hops++)
      n = parent[n];
    if(n != LEGACY_ROOTINO)
      report("directory inode %u is unreachable from the root", inum);
  } else if(!expected){
    report("inode %u has no directory links", inum);
  }
  if(inodes[inum].nlink != expected)
    report("inode %u has link count %u, expected %u", inum, inodes[inum].nlink, expected);
}

int
main(int argc, char **argv)
{
  uint i, files = 0, used = 0, first, bit;
  const char *path = "fs.img";

  if(argc == 2)
    path = argv[1];
  else if(argc != 1){
    fprintf(stderr, "usage: fsck [image]\n");
    return 2;
  }
  if(read_image(path) < 0)
    return 2;

  memcpy(&sb, block(1), sizeof(sb));
  if(!fsgeometry(&sb)){
    report("invalid Project9 superblock or unsupported disk layout");
    return 1;
  }
  if(journal() < 0)
    return 1;

  first = sb.size - sb.nblocks;
  for(i = 1; i < sb.ninodes; i++){
    memcpy(&inodes[i], block(IBLOCK(i,sb)) + (i%IPB)*sizeof(inodes[i]),
           sizeof(inodes[i]));
    check_inode(i);
    if(inodes[i].kind)
      files++;
  }
  if(type(LEGACY_ROOTINO) != 1)
    report("root inode is missing or is not a directory");
  for(i = 1; i < sb.ninodes; i++)
    if(type(i) == 1)
      check_directory(i);
  for(i = 1; i < sb.ninodes; i++)
    check_links(i);
  for(i = 0; i < sb.size; i++){
    bit = (block(BBLOCK(i,sb))[(i%BPB)/8] >> (i%8)) & 1;
    if(bit != (i < first || owner[i] != 0))
      report("block %u allocation bitmap disagrees with metadata", i);
    if(bit && i >= first)
      used++;
  }
  if(errors){
    fprintf(stderr, "fsck: %d filesystem error%s found\n", errors, errors == 1 ? "" : "s");
    return 1;
  }
  printf("fsck: clean: %u allocated inodes, %u/%u data blocks used\n", files, used, sb.nblocks);
  return 0;
}
