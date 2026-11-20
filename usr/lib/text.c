/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "text.h"

int
textline(struct text_input *in, char *line, int cap)
{
  int len = 0, c;

  for(;;){
    if(in->next == in->count){
      in->count = read(in->fd, in->buf, sizeof(in->buf));
      in->next = 0;
      if(in->count < 0)
        return -1;
      if(in->count == 0)
        break;
    }
    c = in->buf[in->next++];
    if(c == '\n'){
      line[len] = 0;
      return 1;
    }
    if(c == 0 || len == cap - 1)
      return -1;
    line[len++] = c;
  }
  line[len] = 0;
  return len ? 1 : 0;
}
