/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "pager.h"
#include "fcntl.h"
#include "termios.h"

int
main(int argc, char **argv)
{
  int i, fd, failed = 0;
  const char *name;

  if(argc == 1 && !isatty(0))
    return pageview(0, "standard input", 0) < 0;
  if(argc == 1){
    printf(2, "usage: less file ... | command | less\n");
    return 2;
  }
  for(i = 1; i < argc; i++){
    name = argv[i];
    fd = !strcmp(name, "-") ? (isatty(0) ? -1 : 0) : open(name, O_RDONLY);
    if(fd < 0 || pageview(fd, name, 0) < 0){
      printf(2, "less: cannot display %s\n", name);
      failed = 1;
    }
    if(fd > 0)
      close(fd);
  }
  return failed;
}
