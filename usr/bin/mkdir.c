/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "user.h"

static int
ensure(const char *path)
{
  struct stat st;

  return mkdir(path) == 0 || (stat(path, &st) == 0 && st.type == T_DIR) ? 0 : -1;
}

static int
parents(const char *path)
{
  char buf[512], *p;

  if(!*path || strlen(path) >= sizeof(buf))
    return -1;
  strcpy(buf, path);
  for(p = buf+1; *p; p++){
    if(*p == '/' && p[-1] != '/'){
      *p = 0;
      if(ensure(buf) < 0)
        return -1;
      *p = '/';
    }
  }
  return ensure(buf);
}

int
main(int argc, char **argv)
{
  int i = 1, createparents = 0, failed = 0;

  if(i < argc && !strcmp(argv[i], "-p")){
    createparents = 1;
    i++;
  }
  if(i < argc && !strcmp(argv[i], "--"))
    i++;
  if(i == argc){
    printf(2, "usage: mkdir [-p] directory ...\n");
    return 2;
  }
  for(; i < argc; i++)
    if((createparents ? parents(argv[i]) : mkdir(argv[i])) < 0){
      printf(2, "mkdir: cannot create %s\n", argv[i]);
      failed = 1;
    }
  return failed;
}
