/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_LOCAL, FD_PTY } type;
  int ref; // reference count
  char readable;
  char writable;
  char append;
  int nonblock;
  struct pipe *pipe;
  struct local *local;
  struct pty *pty;
  int ptymaster;
  struct inode *ip;
  uint off;
};


// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk

  short type;         // copy of disk inode
  short major;
  short minor;
  ushort nlink;
  ushort uid, gid, mode, flags;
  uint size;
  uint addrs[15];
  uint mtime;
  uint data_sectors;
  uint diskflags;
  int orphan;
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int, uint, int);
  int (*available)(int);
  int (*ready)(struct inode*, int);
  int (*write)(struct inode*, char*, int, int);
};

extern struct devsw devsw[];

#include "devices.h"
