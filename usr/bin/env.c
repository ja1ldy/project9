/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "param.h"

static int
length(const char *s)
{
  const char *end = strchr(s, '=');
  return end ? end-s : 0;
}

static int
samekey(const char *a, const char *b, int len)
{
  int i;

  for(i = 0; i < len; i++)
    if(a[i] != b[i])
      return 0;
  return a[i] == '=';
}

int
main(int argc, char **argv)
{
  char *vars[MAXENV+1];
  int i = 1, j, count = 0, n, status;

  if(argc > 1 && !strcmp(argv[1], "-i")){
    i++;
  } else if(environ){
    while(count < MAXENV && environ[count]){
      vars[count] = environ[count];
      count++;
    }
  }
  while(i < argc && (n = length(argv[i])) != 0){
    for(j = 0; j < count && !samekey(vars[j], argv[i], n); j++)
      ;
    if(j == MAXENV){
      printf(2, "env: too many variables\n");
      return 2;
    }
    vars[j] = argv[i++];
    if(j == count)
      count++;
  }
  vars[count] = 0;
  if(i == argc){
    for(j = 0; j < count; j++)
      printf(1, "%s\n", vars[j]);
    return 0;
  }
  environ = vars;
  status = execpath(argv+i, vars, getenv("PATH"));
  printf(2, "env: %s: %s\n", argv[i], status == 126 ? "cannot execute" : "command not found");
  return status;
}
