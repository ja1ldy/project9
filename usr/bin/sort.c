/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "text.h"

struct line {
  struct line *next;
  char text[];
};

static char input[TEXT_LINE];
static int reverse;

static struct line *
merge(struct line *a, struct line *b)
{
  struct line head, **next = &head.next;

  while(a && b){
    if((!reverse && strcmp(a->text, b->text) <= 0) ||
       (reverse && strcmp(a->text, b->text) >= 0)){
      *next = a;
      a = a->next;
    } else {
      *next = b;
      b = b->next;
    }
    next = &(*next)->next;
  }
  *next = a ? a : b;
  return head.next;
}

static struct line *
sort(struct line *first)
{
  struct line *mid, *fast;

  if(!first || !first->next)
    return first;
  mid = first;
  fast = first->next;
  while(fast && fast->next){
    mid = mid->next;
    fast = fast->next->next;
  }
  fast = mid->next;
  mid->next = 0;
  return merge(sort(first), sort(fast));
}

int
main(int argc, char **argv)
{
  struct text_input in = {0};
  struct line *lines = 0, *line;
  int arg = 1, n;

  if(argc > arg && strcmp(argv[arg], "-r") == 0){
    reverse = 1;
    arg++;
  }
  if(argc > arg+1){
    printf(2, "usage: sort [-r] [file]\n");
    exit();
  }
  if(argc == arg+1 && (in.fd = open(argv[arg], O_RDONLY)) < 0){
    printf(2, "sort: cannot open %s\n", argv[arg]);
    exit();
  }
  while((n = textline(&in, input, sizeof(input))) > 0){
    line = malloc(sizeof(*line) + strlen(input) + 1);
    if(!line){
      printf(2, "sort: out of memory\n");
      exit();
    }
    strcpy(line->text, input);
    line->next = lines;
    lines = line;
  }
  if(n < 0){
    printf(2, "sort: read failed or invalid text line\n");
    exit();
  }
  for(line = sort(lines); line; line = line->next){
    n = strlen(line->text);
    if(write(1, line->text, n) != n || write(1, "\n", 1) != 1)
      break;
  }
  exit();
}
