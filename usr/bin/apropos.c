/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fs.h"
#include "text.h"
#include "manual.h"

static int
fold(int c)
{
  return c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
}

static int
contains(const char *line, const char *word)
{
  const char *p, *w;

  for(; *line; line++){
    for(p = line, w = word; *w && *p && fold(*w) == fold(*p); p++, w++)
      ;
    if(!*w)
      return 1;
  }
  return 0;
}

int
main(int argc, char **argv)
{
  struct p9_direntx de;
  struct text_input in;
  char *name = de.name, summary[256];
  int dir, n, section, i, found = 0;

  if(argc < 2){
    printf(2, "usage: apropos keyword ...\n");
    return 2;
  }
  if((dir = open(MANPATH, 0)) < 0){
    printf(2, "apropos: cannot open the manual directory\n");
    return 2;
  }
  while(getdent(dir, &de) > 0){
    n = strlen(name);
    if(n < 3 || name[n-2] != '.')
      continue;
    section = name[n-1] - '0';
    name[n-2] = 0;
    memset(&in, 0, sizeof(in));
    if((in.fd = manual_open(name, section)) < 0)
      continue;
    n = textline(&in, summary, sizeof(summary));
    close(in.fd);
    if(n != 1)
      continue;
    for(i = 1; i < argc; i++)
      if(contains(summary, argv[i])){
        printf(1, "%s(%d) - %s\n", name, section, summary);
        found = 1;
        break;
      }
  }
  close(dir);
  return !found;
}
