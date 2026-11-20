/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_VFS_H
#define PROJECT9_VFS_H

struct inode;
struct p9_direntx;

struct fsops {
  unsigned int (*alloc)(unsigned int, short);
  int (*load)(struct inode *);
  int (*save)(struct inode *);
  void (*drop)(struct inode *);
  int (*read)(struct inode *, char *, unsigned int, unsigned int);
  int (*write)(struct inode *, const char *, unsigned int, unsigned int);
  int (*truncate)(struct inode *);
  int (*writable)(struct inode *);
  int (*lookup)(struct inode *, const char *, unsigned int *, unsigned int *);
  int (*insert)(struct inode *, const char *, unsigned int);
  int (*replace)(struct inode *, unsigned int, const char *, unsigned int);
  int (*next)(struct inode *, unsigned int *, struct p9_direntx *);
};

void vfs_unbind(unsigned int);
void vfs_bind(unsigned int, const struct fsops *);
const struct fsops *vfs_ops(unsigned int);

#endif
