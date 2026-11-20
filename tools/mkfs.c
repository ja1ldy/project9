/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

#include "installpath.h"

#define stat xv6_stat
#define dirent xv6_dirent
#include "types.h"
#include "../include/legacyfs.h"
#include "stat.h"
#include "param.h"
#include "account.h"
#include "mode.h"
#undef dirent
#undef stat

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

#define NINODES NINODE

int nbitmap = FSSIZE/(BSIZE*8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;
int nblocks;

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

ushort xshort(ushort);
void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);

static void
setmode(uint inum, ushort mode)
{
  struct dinode din;

  rinode(inum, &din);
  din.kind = xshort((xshort(din.kind) & INODE_TYPE_MASK) | mode);
  winode(inum, &din);
}

uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar*)&y;
  *a = x & 0xff;
  *(a+1) = (x >> 8) & 0xff;
  *(a+2) = (x >> 16) & 0xff;
  *(a+3) = (x >> 24) & 0xff;
  return y;
}

ushort
xshort(ushort x)
{
  ushort y;
  uchar *a = (uchar*)&y;
  *a = x;
  *(a + 1) = x >> 8;
  return y;
}

static void
fail(const char *message, const char *path)
{
  fprintf(stderr, "mkfs: %s: %s\n", message, path);
  exit(1);
}

static void
entry(uint parent, uint inum, const char *name)
{
  struct xv6_dirent de = {0};

  if(strlen(name) > DIRSIZ)
    fail("name exceeds 14 bytes", name);
  de.inum = xshort(inum);
  memcpy(de.name, name, strlen(name));
  iappend(parent, &de, sizeof(de));
}

static uint
mk_dir(uint parent, const char *name)
{
  uint inum = ialloc(T_DIR);
  struct dinode din;

  entry(inum, inum, ".");
  entry(inum, parent, "..");
  entry(parent, inum, name);
  rinode(parent, &din);
  din.nlink = xshort(xshort(din.nlink) + 1);
  winode(parent, &din);
  return inum;
}

static int have_shadow;

static void
make_shadow(uint etc)
{
  struct account_header header = {0};
  int fd;
  uint inum;

  static_assert(sizeof(header) == BSIZE, "account header must fill one block");
  static_assert(sizeof(struct account_record) == BSIZE, "account record must fill one block");
  fd = open("/dev/urandom", O_RDONLY);
  if(fd < 0 || read(fd, header.seed, sizeof(header.seed)) != sizeof(header.seed)){
    perror("/dev/urandom");
    exit(1);
  }
  close(fd);
  inum = ialloc(T_FILE);
  setmode(inum, I_SHADOW);
  entry(etc, inum, "shadow");
  iappend(inum, &header, sizeof(header));
  have_shadow = 1;
}

static void
add_file(uint parent, const char *path, const char *name, ushort mode)
{
  int n, fd;
  uint inum;
  char buf[BSIZE];

  if((fd = open(path, O_RDONLY)) < 0){
    perror(path);
    exit(1);
  }

  inum = ialloc(T_FILE);
  setmode(inum, mode);
  entry(parent, inum, name);
  while((n = read(fd, buf, sizeof(buf))) > 0)
    iappend(inum, buf, n);
  if(n < 0){
    perror(path);
    exit(1);
  }
  close(fd);
}

static void
install_tree(uint parent, const char *host, const char *guest)
{
  struct dirent **entries;
  struct stat st;
  int i, count, n;
  uint inum;
  const char *name;
  char input[PATH_MAX], dest[PATH_MAX];

  count = scandir(host, &entries, 0, alphasort);
  if(count < 0){
    perror(host);
    exit(1);
  }
  for(i = 0; i < count; i++){
    name = entries[i]->d_name;
    if(!strcmp(name, ".") || !strcmp(name, ".."))
      goto next;
    n = snprintf(input, sizeof(input), "%s/%s", host, name);
    if(n < 0 || n >= sizeof(input))
      fail("source path is too long", host);
    n = snprintf(dest, sizeof(dest), "%s/%s", guest, name);
    if(n < 0 || n >= sizeof(dest))
      fail("guest path is too long", guest);
    if(!strcmp(dest, "/etc/shadow"))
      fail("reserved for the account database", input);
    if(lstat(input, &st) < 0){
      perror(input);
      exit(1);
    }
    if(S_ISDIR(st.st_mode)){
      inum = mk_dir(parent, name);
      if(!strcmp(dest, "/root"))
        setmode(inum, 0700);
      else if(!strcmp(dest, "/tmp"))
        setmode(inum, 01777);
      else if(!strcmp(dest, "/etc"))
        make_shadow(inum);
      install_tree(inum, input, dest);
    } else if(S_ISREG(st.st_mode)){
      if(st.st_size > (off_t)MAXFILE * BSIZE)
        fail("file exceeds the filesystem size limit", input);
      add_file(parent, input, name, !strcmp(guest, "/bin") ? 0755 : 0644);
    } else {
      fail("expected a regular file or directory", input);
    }
  next:
    free(entries[i]);
  }
  free(entries);
}

// keep the old file list interface for callers that build their own images
static void
install_list(uint root, int count, char *paths[])
{
  uint bin = mk_dir(root, "bin");
  mk_dir(root, "dev");
  uint etc = mk_dir(root, "etc");
  mk_dir(root, "home");
  setmode(mk_dir(root, "root"), 0700);
  setmode(mk_dir(root, "tmp"), 01777);
  uint man = mk_dir(mk_dir(mk_dir(root, "usr"), "share"), "man");
  mk_dir(root, "var");
  make_shadow(etc);

  for(int i = 0; i < count; i++){
    const char *path = paths[i];
    const char *name = strrchr(path, '/');
    uint parent = root;
    ushort mode = 0644;

    name = name ? name+1 : path;
    if(in_directory(path, "bin")){
      parent = bin;
      mode = 0755;
    } else if(is_manual_path(path)){
      parent = man;
    } else if(in_directory(path, "etc") || !strcmp(name, "motd") ||
              !strcmp(name, "hostname.txt")){
      parent = etc;
    }
    if(!strcmp(name, "hostname.txt"))
      name = "hostname";
    add_file(parent, path, name, mode);
  }
}

int
main(int argc, char *argv[])
{
  int i, tree;
  uint rootino;
  char buf[BSIZE];

  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  tree = argc >= 3 && !strcmp(argv[2], "--root");
  if(argc < 2 || (tree && argc != 4)){
    fprintf(stderr, "usage: mkfs fs.img [--root directory | files ...]\n");
    exit(1);
  }

  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct xv6_dirent)) == 0);

  fsfd = open(*(argv + 1), O_RDWR|O_CREAT|O_TRUNC, 0666);
  if(fsfd < 0){
    perror(*(argv + 1));
    exit(1);
  }

  nmeta = 2 + nlog + ninodeblocks + nbitmap;
  nblocks = FSSIZE - nmeta;

  sb.size = xint(FSSIZE);
  sb.nblocks = xint(nblocks);
  sb.ninodes = xint(NINODES);
  sb.nlog = xint(nlog);
  sb.logstart = xint(2);
  sb.inodestart = xint(2+nlog);
  sb.bmapstart = xint(2+nlog+ninodeblocks);
  sb.magic = xint(FS_MAGIC);

  freeblock = nmeta;

  for(i = 0; i < FSSIZE; i++)
    wsect(i, zeroes);

  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);

  rootino = ialloc(T_DIR);
  assert(rootino == LEGACY_ROOTINO);

  entry(rootino, rootino, ".");
  entry(rootino, rootino, "..");

  if(tree)
    install_tree(rootino, argv[3], "");
  else
    install_list(rootino, argc-2, argv+2);
  if(!have_shadow)
    fail("rootfs is missing the etc directory", argv[3]);

  balloc(freeblock);
  exit(0);
}

void
wsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    perror("lseek");
    exit(1);
  }
  if(write(fsfd, buf, BSIZE) != BSIZE){
    perror("write");
    exit(1);
  }
}

void
winode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}

void
rinode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *ip = *dip;
}

void
rsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    perror("lseek");
    exit(1);
  }
  if(read(fsfd, buf, BSIZE) != BSIZE){
    perror("read");
    exit(1);
  }
}

uint
ialloc(ushort type)
{
  uint inum;
  struct dinode din;

  if(freeinode >= NINODES)
    fail("out of inodes", "root filesystem");
  inum = freeinode++;

  bzero(&din, sizeof(din));
  din.kind = xshort((type << INODE_TYPE_SHIFT) | (type == T_DIR ? 0755 : 0644));
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

void
balloc(int used)
{
  uchar buf[BSIZE];
  int i;

  bzero(buf, BSIZE);
  for(i = 0; i < used; i++){
    buf[i/8] = buf[i/8] | (0x1 << (i%8));
  }
  wsect(sb.bmapstart, buf);
}

static uint
alloc_block(void)
{
  if(freeblock >= FSSIZE)
    fail("out of data blocks", "root filesystem");
  return freeblock++;
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void
iappend(uint inum, void *xp, int n)
{
  char *p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);
  off = xint(din.size);
  while(n > 0){
    fbn = off / BSIZE;
    if(fbn >= MAXFILE)
      fail("file is too large", "root filesystem");
    if(fbn < NDIRECT){
      if(xint(*(din.addrs + fbn)) == 0){
        *(din.addrs + fbn) = xint(alloc_block());
      }
      x = xint(*(din.addrs + fbn));
    } else {
      if(xint(din.addrs[NDIRECT]) == 0){
        din.addrs[NDIRECT] = xint(alloc_block());
      }
      rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      if(*(indirect + fbn - NDIRECT) == 0){
        *(indirect + fbn - NDIRECT) = xint(alloc_block());
        wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      }
      x = xint(*(indirect + fbn - NDIRECT));
    }
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}
