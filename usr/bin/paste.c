/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "param.h"
#include "text.h"

#define MAX_FILES (NOFILE - 3)

static struct text_input inputs[MAX_FILES];
static char records[MAX_FILES][TEXT_LINE];
static int ended[MAX_FILES];

int
main(int argc, char **argv)
{
  int i = 1, count, col, any, n, stdin_seen = 0;
  char delimiter = '\t';

  if(argc > 1 && !strcmp(argv[1], "-d")){
    if(argc < 3 || strlen(argv[2]) != 1){
      printf(2, "usage: paste [-d delimiter] [file ...]\n");
      return 2;
    }
    delimiter = argv[2][0];
    i += 2;
  }
  count = argc - i;
  if(count > MAX_FILES){
    printf(2, "paste: too many input files\n");
    return 2;
  }
  for(col = 0; col < count; col++, i++){
    if(!strcmp(argv[i], "-")){
      if(stdin_seen++){
        printf(2, "paste: use standard input at most once\n");
        return 2;
      }
    } else if((inputs[col].fd = open(argv[i], O_RDONLY)) < 0){
      printf(2, "paste: cannot open %s\n", argv[i]);
      return 1;
    }
  }
  if(!count)
    count = 1;
  for(;;){
    any = 0;
    for(col = 0; col < count; col++){
      records[col][0] = 0;
      if(ended[col])
        continue;
      n = textline(&inputs[col], records[col], TEXT_LINE);
      if(n < 0){
        printf(2, "paste: input error or line too long\n");
        return 1;
      }
      ended[col] = (n == 0);
      any |= !ended[col];
    }
    if(!any)
      return 0;
    for(col = 0; col < count; col++){
      n = strlen(records[col]);
      if((col && write(1, &delimiter, 1) != 1) || write(1, records[col], n) != n)
        return 1;
    }
    if(write(1, "\n", 1) != 1)
      return 1;
  }
}
