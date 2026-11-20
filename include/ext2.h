/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_EXT2_H
#define PROJECT9_EXT2_H

#define EXT2DEV 2
#define EXT2ROOT 2
#define INSTALLDEV 3

struct inode;
struct p9_direntx;

void ext2init(void);
void ext2mountdata(void);
void fsopsinit(void);
int ext2sync(void);
struct ext2stats;
void ext2stats(struct ext2stats *);
uint ext2mountpoint(void);
uint ext2host(uint);
int ext2installmount(uint);
int ext2installunmount(void);
int ext2maywrite(struct inode *);
int ext2load(struct inode *);
int ext2save(struct inode *);
int ext2read(struct inode *, char *, uint, uint);
int ext2write(struct inode *, const char *, uint, uint);
int ext2truncate(struct inode *);
uint ext2alloc(uint, short);
void ext2free(struct inode *);
int ext2next(struct inode *, uint *, struct p9_direntx *);
int ext2lookup(struct inode *, const char *, uint *, uint *);
int ext2insert(struct inode *, const char *, uint);
int ext2setentry(struct inode *, uint, const char *, uint);

uint ataextprobe(uint);
int ataextrw(uint, uint, void *, int);

#endif
