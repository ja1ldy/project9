/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

char **environ;

char *
getenv(const char *name)
{
  char **entry;
  const char *p, *n;

  if(!environ || !*name)
    return 0;
  for(entry = environ; *entry; entry++){
    p = *entry;
    for(n = name; *n && *n == *p; n++, p++)
      ;
    if(!*n && *p == '=')
      return (char*)p + 1;
  }
  return 0;
}

int
exec(char *path, char **argv)
{
  return execve(path, argv, environ);
}
