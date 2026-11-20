/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "diskuser.h"

#define BLOCK 4096
#define INODES 1024
#define TABLE 4
#define FIRST_DATA (TABLE+INODES*128/BLOCK)
#define ALLOCATED (FIRST_DATA+3)
#define FREE_INODES (INODES-12)

static uchar block[BLOCK];

static int
writeblock(int id, uint n)
{
  return diskctl(DC_WRITE_PART, id, n*8, 8, block);
}

static void
entry(uint off, uint number, uint room, char *name, uchar kind)
{
  dput(block+off, number, 4);
  dput(block+off+4, room, 2);
  block[off+6] = strlen(name);
  block[off+7] = kind;
  memmove(block+off+8, name, block[off+6]);
}

static void
inode(uint number, uint mode, uint links, uint size, uint first)
{
  uchar *p = block + ((number-1)%32)*128;

  dput(p, mode, 2);
  dput(p+4, size, 4);
  dput(p+26, links, 2);
  dput(p+28, 8, 4);
  dput(p+40, first, 4);
}

int
main(int argc, char **argv)
{
  struct diskinfo disk;
  int id;
  uint blocks;
  uchar seed[32], *super;

  if(argc != 2 || (id = diskid(argv[1], 1)) < 0){
    printf(2, "usage: newfs /dev/hd1a\n");
    return 2;
  }
  if(diskctl(DC_INFO, id, 0, 1, &disk) < 0 || !(disk.flags & DC_TEST) ||
     disk.first != DC_START || disk.count < DC_MIN || disk.count > DC_LIMIT ||
     (disk.count & 7) || diskctl(DC_SEED, id, 0, 1, seed) < 0)
    goto bad;
  blocks = disk.count/8;

  // publish the superblock only after all other metadata is on disk
  if(writeblock(id, 0) < 0)
    goto bad;
  dput(block, 2, 4);
  dput(block+4, 3, 4);
  dput(block+8, TABLE, 4);
  dput(block+12, blocks-ALLOCATED, 2);
  dput(block+14, FREE_INODES, 2);
  dput(block+16, 2, 2);
  if(writeblock(id, 1) < 0)
    goto bad;

  memset(block, 0, BLOCK);
  for(uint n = 0; n < BLOCK*8; n++)
    if(n < ALLOCATED || n >= blocks)
      block[n/8] |= 1 << (n%8);
  if(writeblock(id, 2) < 0)
    goto bad;
  memset(block, 255, BLOCK);
  memset(block, 0, INODES/8);
  block[0] = 255;
  block[1] = 15;
  if(writeblock(id, 3) < 0)
    goto bad;

  memset(block, 0, BLOCK);
  inode(2, 0040755, 3, BLOCK, FIRST_DATA);
  inode(11, 0040755, 2, BLOCK, FIRST_DATA+1);
  inode(12, 0100000, 1, 512, FIRST_DATA+2);
  if(writeblock(id, TABLE) < 0)
    goto bad;
  memset(block, 0, BLOCK);
  for(uint n = TABLE+1; n < FIRST_DATA; n++)
    if(writeblock(id, n) < 0)
      goto bad;

  entry(0, 2, 12, ".", 2);
  entry(12, 2, 12, "..", 2);
  entry(24, 11, BLOCK-24, "etc", 2);
  if(writeblock(id, FIRST_DATA) < 0)
    goto bad;
  memset(block, 0, BLOCK);
  entry(0, 11, 12, ".", 2);
  entry(12, 2, 12, "..", 2);
  entry(24, 12, BLOCK-24, "shadow", 1);
  if(writeblock(id, FIRST_DATA+1) < 0)
    goto bad;
  memset(block, 0, BLOCK);
  memmove(block, seed, sizeof(seed));
  if(writeblock(id, FIRST_DATA+2) < 0)
    goto bad;

  memset(block, 0, BLOCK);
  super = block+1024;
  dput(super, INODES, 4);
  dput(super+4, blocks, 4);
  dput(super+12, blocks-ALLOCATED, 4);
  dput(super+16, FREE_INODES, 4);
  dput(super+24, 2, 4);
  dput(super+28, 2, 4);
  dput(super+32, BLOCK*8, 4);
  dput(super+36, BLOCK*8, 4);
  dput(super+40, INODES, 4);
  dput(super+54, 0xffff, 2);
  dput(super+56, 0xef53, 2);
  dput(super+58, 1, 2);
  dput(super+60, 1, 2);
  dput(super+76, 1, 4);
  dput(super+84, 11, 4);
  dput(super+88, 128, 2);
  dput(super+96, 2, 4);
  memmove(super+104, seed+16, 16);
  super[110] = (super[110] & 15) | 0x40;
  super[112] = (super[112] & 63) | 0x80;
  if(writeblock(id, 0) < 0)
    goto bad;
  printf(1, "newfs: ext2, %u blocks of %u bytes\n", blocks, BLOCK);
  return 0;
bad:
  printf(2, "newfs: cannot format the selected test partition\n");
  return 1;
}
