/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "stat.h"

#define EXEC_PATH_CAP 2047

int
execpath(char **argv, char **envp, const char *path)
{
  const char *dir;
  char *file;
  uint n, len = strlen(argv[0]);
  struct statx info;
  int found = 0;

  if(!len)
    return 127;
  if(strchr(argv[0], '/')){
    execve(argv[0], argv, envp);
    return statx(argv[0], &info) == 0 ? 126 : 127;
  }
  if(!(file = malloc(EXEC_PATH_CAP + 1)))
    return 126;
  if(!path)
    path = "/bin:/usr/bin:/usr/X11/bin";
  do {
    for(dir = path; *path && *path != ':'; path++)
      ;
    n = path - dir;
    if(n + len + 1 <= EXEC_PATH_CAP){
      if(n){
        memmove(file, dir, n);
        file[n++] = '/';
      }
      strcpy(file + n, argv[0]);
      execve(file, argv, envp);
      if(statx(file, &info) == 0)
        found = 1;
    }
  } while(*path && *path++ == ':');
  free(file);
  return found ? 126 : 127;
}
