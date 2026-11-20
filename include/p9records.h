/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_P9RECORDS_H
#define PROJECT9_P9RECORDS_H

#define P9_T_DIR 1
#define P9_T_FILE 2
#define P9_T_DEV 3
#define P9_T_SOCKET 4

#define P9_STAT_FIELDS \
  short type; \
  int dev; \
  unsigned int ino; \
  short nlink; \
  unsigned int size

#define P9_STATX_FIELDS(stat_type) \
  struct stat_type st; \
  unsigned int uid, gid, mode

#define P9_DIRSIZ 14
#define P9_DIRENT_FIELDS \
  unsigned short inum; \
  char name[P9_DIRSIZ]

struct p9_stat_record { P9_STAT_FIELDS; };
struct p9_statx_record { P9_STATX_FIELDS(p9_stat_record); };
struct p9_dirent_record { P9_DIRENT_FIELDS; };

#define P9_NAME_MAX 255
#define P9_NAME_BUFSIZE (P9_NAME_MAX + 2)
struct p9_direntx {
  unsigned int inum;
  char name[P9_NAME_MAX + 1];
};

#endif
