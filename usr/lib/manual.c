/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "manual.h"

int
manual_open(const char *topic, int section)
{
  char path[sizeof(MANPATH) + P9_NAME_MAX + 1];
  const char *p;
  char *name;
  int count;

  strcpy(path, MANPATH "/");
  name = path + sizeof(MANPATH);
  for(p = topic, count = 0; *p; p++, count++){
    if(!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
         *p == '_' || *p == '-' || *p == '[') || count >= P9_NAME_MAX - 2)
      return -1;
    name[count] = *p;
  }
  if(!count || (section != 1 && section != 2 && section != 5))
    return -1;
  name[count++] = '.';
  name[count++] = '0' + section;
  name[count] = 0;
  return open(path, O_RDONLY);
}
