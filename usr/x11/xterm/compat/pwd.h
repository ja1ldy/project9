/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LOCAL_PWD_H
#define P9_LOCAL_PWD_H
#include <sys/types.h>
struct passwd {
  char *pw_name, *pw_passwd;
  uid_t pw_uid;
  gid_t pw_gid;
  char *pw_gecos, *pw_dir, *pw_shell;
};
struct passwd *getpwnam(const char *);
struct passwd *getpwuid(uid_t);
#endif
