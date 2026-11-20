/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

static uchar table[256], deleted[256], buf[512];

static int
expand(const char *set, uchar *items)
{
  int len = 0, ch, last;

  while(*set){
    ch = (uchar)*set++;
    last = ch;
    if(set[0] == '-' && set[1]){
      last = (uchar)set[1];
      set += 2;
    }
    if(last < ch || last-ch+1 > 256-len)
      return -1;
    while(ch <= last)
      items[len++] = ch++;
  }
  return len;
}

int
main(int argc, char **argv)
{
  uchar src[256], dst[256];
  int n, m, i, count, deleting = 0;

  if(argc != 3)
    goto usage;
  deleting = strcmp(argv[1], "-d") == 0;
  if(deleting){
    n = expand(argv[2], src);
    m = 0;
  } else {
    n = expand(argv[1], src);
    m = expand(argv[2], dst);
  }
  if(n <= 0 || (!deleting && m <= 0))
    goto usage;
  for(i = 0; i < 256; i++)
    table[i] = i;
  for(i = 0; i < n; i++){
    if(deleting)
      deleted[src[i]] = 1;
    else
      table[src[i]] = dst[i < m ? i : m-1];
  }
  while((n = read(0, buf, sizeof(buf))) > 0){
    count = 0;
    for(i = 0; i < n; i++)
      if(!deleted[buf[i]])
        buf[count++] = table[buf[i]];
    if(write(1, buf, count) != count)
      exit();
  }
  if(n < 0)
    printf(2, "tr: read failed\n");
  exit();

usage:
  printf(2, "usage: tr set1 set2 | tr -d set (literal bytes or ranges such as a-z)\n");
  exit();
}
