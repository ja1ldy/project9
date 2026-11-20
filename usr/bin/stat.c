/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char **argv)
{
  struct statx info;
  struct stat *st = &info.st;
  const char *type;
  char mode[5];
  int i, failed = 0;

  if(argc < 2){
    printf(2, "usage: stat file ...\n");
    exitstatus(1);
  }
  for(i = 1; i < argc; i++){
    if(statx(argv[i], &info) < 0){
      printf(2, "stat: cannot stat %s\n", argv[i]);
      failed = 1;
      continue;
    }
    formatmode(info.mode, mode);
    type = st->type == T_DIR ? "directory" : st->type == T_DEV ? "device" : "file";
    printf(1, "%s: %s, size=%d, inode=%d, links=%d, device=%d, uid=%u, gid=%u, mode=%s\n",
           argv[i], type, st->size, st->ino, st->nlink, st->dev, info.uid, info.gid, mode);
  }
  exitstatus(failed);
}
