/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_FILEMAP_H
#define PROJECT9_FILEMAP_H

#define FILEMAP_BASE 0x40000000U
#define FILEMAP_COUNT 64
#define FILEMAP_LIMIT (128 * 1024 * 1024U)

struct filemap { unsigned int start, length; };
struct proc;
struct inode;

unsigned int usermapend(struct proc *, unsigned int);
int private_map(struct proc *, struct inode *, unsigned int, unsigned int,
                unsigned int, int);
int private_unmap(struct proc *, unsigned int, unsigned int);
int private_clone(struct proc *, struct proc *);
void private_reset(struct proc *);

#endif
