/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_STAT_H
#define P9_LIBC_SYS_STAT_H
#include <sys/types.h>
#include <project9/p9records.h>
#define S_IFMT 0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFSOCK 0140000
#define S_ISSOCK(m) (((m)&S_IFMT)==S_IFSOCK)
#define S_ISREG(m) (((m)&S_IFMT)==S_IFREG)
#define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
#define S_ISCHR(m) (((m)&S_IFMT)==S_IFCHR)
#define DEFFILEMODE 0666U
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
struct stat {
  dev_t st_dev, st_rdev;
  ino_t st_ino;
  mode_t st_mode;
  nlink_t st_nlink;
  uid_t st_uid;
  gid_t st_gid;
  off_t st_size;
  long st_blksize;
};
int stat(const char *, struct stat *) __asm__("p9_stat");
int fstat(int, struct stat *) __asm__("p9_fstat");
int mkdir(const char *, mode_t) __asm__("p9_mkdir");
int chmod(const char *, mode_t) __asm__("p9_chmod");
int fchmod(int, mode_t) __asm__("p9_fchmod");
int chown(const char *, uid_t, gid_t) __asm__("p9_chown");
mode_t umask(mode_t) __asm__("p9_umask");
int filemtime(const char *, time_t *) __asm__("p9_filemtime");
#endif
