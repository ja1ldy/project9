/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

static int
copy(const char *source, const char *target)
{
  int from, to, n, failed = 0;
  struct stat src, dst;
  char buf[512];

  from = open(source, O_RDONLY);
  if(from < 0 || fstat(from, &src) < 0 || src.type != T_FILE){
    printf(2, "cp: cannot read regular file %s\n", source);
    if(from >= 0)
      close(from);
    return 1;
  }
  if(stat(target, &dst) == 0 && dst.ino == src.ino && dst.dev == src.dev){
    printf(2, "cp: %s and %s are the same file\n", source, target);
    close(from);
    return 1;
  }
  to = open(target, O_CREATE | O_WRONLY | O_TRUNC);
  if(to < 0){
    printf(2, "cp: cannot create %s\n", target);
    close(from);
    return 1;
  }
  while((n = read(from, buf, sizeof(buf))) > 0){
    if(write(to, buf, n) != n){
      failed = 1;
      break;
    }
  }
  failed |= n < 0;
  if(failed)
    printf(2, "cp: cannot copy %s to %s\n", source, target);
  close(from);
  close(to);
  return failed;
}

int
main(int argc, char **argv)
{
  const char *target, *p, *base;
  char path[512];
  struct stat st;
  int i, first = 1, dir, failed = 0;
  uint len;

  if(argc > 1 && !strcmp(argv[1], "--"))
    first++;
  if(argc - first < 2){
    printf(2, "usage: cp source ... target\n");
    return 2;
  }
  target = argv[argc-1];
  dir = stat(target, &st) == 0 && st.type == T_DIR;
  if(!dir && argc - first > 2){
    printf(2, "cp: %s is not a directory\n", target);
    return 1;
  }
  for(i = first; i < argc-1; i++){
    if(!dir){
      failed |= copy(argv[i], target);
      continue;
    }
    for(base = p = argv[i]; *p; p++)
      if(*p == '/')
        base = p + 1;
    len = strlen(target);
    if(strlen(base) + len + 2 > sizeof(path) || !*base){
      printf(2, "cp: invalid pathname %s\n", argv[i]);
      failed = 1;
      continue;
    }
    strcpy(path, target);
    if(len && path[len-1] != '/')
      path[len++] = '/';
    strcpy(path+len, base);
    failed |= copy(argv[i], path);
  }
  return failed;
}
