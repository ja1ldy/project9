/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "diskuser.h"

#define BLOCK 4096
#define INODES 1024
#define DATA 36

static uchar meta[4][BLOCK], owners[BLOCK], used[BLOCK], inodeblock[BLOCK];
static uchar tree[4][BLOCK];
static ushort kinds[INODES+1], links[INODES+1], parent[INODES+1], dotdot[INODES+1];
static uint refs[INODES+1];
static uchar dots[INODES+1];
static uint blocks, dirs, files, count;
static int disk, errors;

static int
io(uint block, void *buffer, int write)
{
  if(block >= blocks ||
     diskctl(write ? DC_WRITE_PART : DC_READ_PART, disk, block*8, 8, buffer) < 0){
    if(!errors++)
      printf(2, "fsck: disk I/O failed at block %u\n", block);
    return -1;
  }
  return 0;
}

static int
mark(uchar *map, uint bit)
{
  uint mask = 1U << (bit%8);
  int old = map[bit/8] & mask;
  map[bit/8] |= mask;
  return old != 0;
}

static int
directory(uint n, uchar *block)
{
  uint p, ino, len, namelen, type, expected;
  uchar *name;

  for(p = 0; p < BLOCK; p += len){
    if(BLOCK-p < 8)
      return -1;
    ino = dget(block+p, 4);
    len = dget(block+p+4, 2);
    namelen = block[p+6];
    type = block[p+7];
    name = block+p+8;
    if(len < 8 || len % 4 || len > BLOCK-p || namelen > len-8)
      return -1;
    if(!ino)
      continue;
    if(!namelen || ino > INODES || !kinds[ino])
      return -1;
    expected = kinds[ino] == 0040000 ? 2 : kinds[ino] == 0120000 ? 7 :
               kinds[ino] == 0020000 ? 3 : kinds[ino] == 0140000 ? 6 : 1;
    if(type && type != expected)
      return -1;
    for(uint i = 0; i < namelen; i++)
      if(!name[i] || name[i] == '/')
        return -1;
    refs[ino]++;
    if(namelen == 1 && name[0] == '.'){
      if(ino != n || dots[n])
        return -1;
      dots[n] = 1;
    } else if(namelen == 2 && name[0] == '.' && name[1] == '.'){
      if(dotdot[n] || kinds[ino] != 0040000)
        return -1;
      dotdot[n] = ino;
    } else if(kinds[ino] == 0040000){
      if(ino == 2 || parent[ino])
        return -1;
      parent[ino] = n;
    }
  }
  return 0;
}

static int
walk(uint inode, uint sector, uint depth, uint logical, uint length)
{
  uint next, i, span = 1;
  int dir = kinds[inode] == 0040000;

  if(!sector)
    return dir && logical < length ? -1 : 0;
  if(logical >= length || sector < DATA || sector >= blocks || mark(owners, sector))
    return -1;
  count++;
  if(!depth)
    return dir && (io(sector, tree[0], 0) < 0 || directory(inode, tree[0]) < 0) ? -1 : 0;
  if(io(sector, tree[depth], 0) < 0)
    return -1;
  for(i = 1; i < depth; i++)
    span *= BLOCK/4;
  for(i = 0; i < BLOCK/4; i++){
    next = dget(tree[depth]+4*i, 4);
    if(walk(inode, next, depth-1, logical+i*span, length) < 0)
      return -1;
  }
  return 0;
}

static uint
inodes(int scan)
{
  uint i, slot, table, mode, length, depth, offset, next;
  uchar *p;

  for(table = 4; table < DATA; table++){
    if(io(table, inodeblock, 0) < 0)
      return 1;
    for(slot = 0; slot < BLOCK/128; slot++){
      i = (table-4)*(BLOCK/128)+slot+1;
      p = inodeblock+slot*128;
      mode = dget(p, 2) & 0170000;
      if(!scan){
        if(i < 11)
          mark(used, i-1);
        if(!mode)
          continue;
        if((i < 11 && i != 2) || dget(p+104, 4) || dget(p+108, 4) ||
           (mode != 0020000 && mode != 0040000 && mode != 0100000 && mode != 0120000 &&
            mode != 0140000) ||
           !(links[i] = dget(p+26, 2)))
          return i;
        kinds[i] = mode;
        if(i >= 11)
          mark(used, i-1);
        dirs += mode == 0040000;
        files++;
        continue;
      }
      if(!kinds[i])
        continue;
      length = dget(p+4, 4);
      count = 0;
      if(length > 0x7fffffff || (mode == 0040000 && (!length || length % BLOCK)))
        return i;
      if(mode == 0020000 || mode == 0140000){
        if(length || dget(p+28, 4))
          return i;
        continue;
      }
      if(mode == 0120000 && !dget(p+28, 4)){
        if(!length || length > 60)
          return i;
        continue;
      }
      length = (length + BLOCK-1)/BLOCK;
      for(next = 0; next < 12; next++)
        if(walk(i, dget(p+40+next*4, 4), 0, next, length) < 0)
          return i;
      for(depth = 1, offset = 12, next = 1; depth <= 3; depth++){
        if(walk(i, dget(p+84+4*depth, 4), depth, offset, length) < 0)
          return i;
        next *= BLOCK/4;
        offset += next;
      }
      if(dget(p+28, 4) != count*8)
        return i;
    }
  }
  return 0;
}

int
main(int argc, char **argv)
{
  struct diskinfo info;
  uchar *sb = meta[0]+1024, *g = meta[1];
  uint i, n, fb = 0, fi = 0;
  int fix = argc == 3 && !strcmp(argv[1], "-y");

  if((argc != 2 && !fix) || (disk = diskid(argv[argc-1], 1)) < 0){
    printf(2, "usage: fsck [-y] partition\n");
    return 2;
  }
  if(diskctl(DC_INFO, disk, 0, 1, &info) < 0 ||
     info.count < DC_MIN || info.count > DC_LIMIT || (info.count & 7))
    goto layout;
  blocks = info.count/8;
  for(i = 0; i < 4; i++)
    if(io(i, meta[i], 0) < 0)
      return 1;
  if(dget(sb+56, 2) != 0xef53)
    goto layout;
  if(dget(sb, 4) != INODES || dget(sb+4, 4) != blocks || dget(sb+20, 4) ||
     dget(sb+24, 4) != 2 || dget(sb+28, 4) != 2 ||
     dget(sb+32, 4) != BLOCK*8 || dget(sb+36, 4) != BLOCK*8 ||
     dget(sb+40, 4) != INODES || dget(sb+76, 4) != 1 ||
     dget(sb+84, 4) != 11 || dget(sb+88, 2) != 128 ||
     dget(sb+92, 4) || dget(sb+96, 4) != 2 || dget(sb+100, 4) ||
     dget(g, 4) != 2 || dget(g+4, 4) != 3 || dget(g+8, 4) != 4)
    goto layout;
  for(i = 0; i < BLOCK*8; i++){
    if(i < DATA || i >= blocks)
      mark(owners, i);
    if(i >= INODES)
      mark(used, i);
  }
  n = inodes(0);
  if(!n)
    n = inodes(1);
  if(n || kinds[2] != 0040000){
    printf(2, "fsck: invalid inode or block ownership at inode %u\n", n ? n : 2);
    return 1;
  }
  parent[2] = 2;
  for(i = 1; i <= INODES; i++){
    if(!kinds[i])
      continue;
    if(refs[i] != links[i] || (kinds[i] == 0040000 && (!parent[i] || parent[i] != dotdot[i] || !dots[i]))){
      printf(2, "fsck: bad link count or parent for inode %u\n", i);
      return 1;
    }
    if(kinds[i] == 0040000){
      uint p = i;
      for(n = 0; p != 2 && n < INODES; n++)
        p = parent[p];
      if(p != 2){
        printf(2, "fsck: directory is disconnected\n");
        return 1;
      }
    }
  }
  for(i = 0; i < blocks; i++)
    fb += !(owners[i/8] & (1U << (i%8)));
  for(i = 0; i < INODES; i++)
    fi += !(used[i/8] & (1U << (i%8)));
  if(dget(sb+12, 4) != fb || dget(sb+16, 4) != fi || dget(sb+58, 2) != 1 ||
     dget(g+12, 2) != fb || dget(g+14, 2) != fi || dget(g+16, 2) != dirs ||
     dcompare(owners, meta[2], BLOCK) || dcompare(used, meta[3], BLOCK)){
    if(!fix){
      printf(2, "fsck: allocation summary or clean flag differs\n");
      return 1;
    }
    dput(sb+58, 0, 2);
    if(io(0, meta[0], 1) < 0)
      return 1;
    dput(sb+12, fb, 4);
    dput(sb+16, fi, 4);
    dput(g+12, fb, 2);
    dput(g+14, fi, 2);
    dput(g+16, dirs, 2);
    if(io(2, owners, 1) < 0 || io(3, used, 1) < 0 || io(1, meta[1], 1) < 0)
      return 1;
    // verify the repaired metadata before publishing the clean flag
    for(i = 1; i <= 3; i++){
      uchar *expected = i == 1 ? meta[1] : i == 2 ? owners : used;
      if(io(i, tree[0], 0) < 0 || dcompare(expected, tree[0], BLOCK)){
        printf(2, "fsck: repair could not be verified at block %u\n", i);
        return 1;
      }
    }
    dput(sb+58, 1, 2);
    if(io(0, meta[0], 1) < 0)
      return 1;
    printf(1, "fsck: repaired allocation metadata\n");
  }
  printf(1, "fsck: ext2 clean, %u inodes and %u blocks used\n", files, blocks-fb);
  return 0;
layout:
  printf(2, "fsck: expected a Project9 newfs ext2 partition\n");
  return 2;
}
