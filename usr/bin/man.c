/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "manual.h"
#include "pager.h"

int
main(int argc, char **argv)
{
  static const uint sections[] = {1, 2, 5};
  const char *topic;
  int fd = -1, result;
  uint section, i;

  if(argc == 3){
    if(parseuint(argv[1], &section) < 0 || (section != 1 && section != 2 && section != 5))
      goto usage;
    fd = manual_open(argv[2], section);
  } else if(argc == 2){
    for(i = 0; i < sizeof(sections) / sizeof(*sections); i++){
      section = sections[i];
      if((fd = manual_open(argv[1], section)) >= 0)
        break;
    }
  } else {
    goto usage;
  }
  topic = argv[argc-1];
  if(fd < 0){
    printf(2, "man: no entry for %s\n", topic);
    return 1;
  }
  result = pageview(fd, topic, section);
  close(fd);
  if(result < 0)
    printf(2, "man: cannot display %s\n", topic);
  return result < 0;

usage:
  printf(2, "usage: man [section] topic\n");
  return 2;
}
