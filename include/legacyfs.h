/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "fs.h"

// On-disk file system format.
// Both the kernel and user programs use this header file.


#define LEGACY_ROOTINO 1
#define FSSIZE 4000
#define MAXOPBLOCKS 10
#define LOGSIZE (MAXOPBLOCKS * 3)
#define NBUF LOGSIZE

// Disk layout:
// boot block super block log inode blocks
// free bit map data blocks
//
// mkfs computes the super block and builds an initial file system the
// super block describes the disk layout
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // block number of first log block
  uint inodestart;   // block number of first inode block
  uint bmapstart;    // block number of first free map block
  uint magic;        // project9 inode format
};

#define FS_MAGIC 0x32534650U
#define INODE_TYPE_SHIFT 12
#define INODE_TYPE_MASK 0x3000

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  ushort kind;          // type permissions and the shadow flag
  ushort nlink;
  ushort uid;
  ushort gid;
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // data blocks or major and minor for a device
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// block of free map containing bit for block b
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ P9_DIRSIZ

struct dirent { P9_DIRENT_FIELDS; };
