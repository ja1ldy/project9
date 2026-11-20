/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "text.h"

static char previous[TEXT_LINE], current[TEXT_LINE];

static void
output(const char *line, int count, int counts)
{
  if(counts)
    printf(1, "%d ", count);
  write(1, line, strlen(line));
  write(1, "\n", 1);
}

int
main(int argc, char **argv)
{
  struct text_input in = {0};
  int arg = 1, counts = 0, n, count = 0;

  if(argc > arg && strcmp(argv[arg], "-c") == 0){
    counts = 1;
    arg++;
  }
  if(argc > arg + 1){
    printf(2, "usage: uniq [-c] [file]\n");
    exit();
  }
  if(argc == arg + 1 && (in.fd = open(argv[arg], O_RDONLY)) < 0){
    printf(2, "uniq: cannot open %s\n", argv[arg]);
    exit();
  }
  while((n = textline(&in, current, sizeof(current))) > 0){
    if(count && strcmp(previous, current) != 0){
      output(previous, count, counts);
      count = 0;
    }
    if(!count)
      strcpy(previous, current);
    count++;
  }
  if(count)
    output(previous, count, counts);
  if(n < 0)
    printf(2, "uniq: read failed or invalid text line\n");
  exit();
}
