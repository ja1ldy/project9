/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char **argv)
{
  int a, b, n, m, i, offset = 0;
  char x[512], y[512];

  if(argc != 3){
    printf(2, "usage: cmp file1 file2\n");
    return 2;
  }
  a = open(argv[1], O_RDONLY);
  b = open(argv[2], O_RDONLY);
  if(a < 0 || b < 0){
    printf(2, "cmp: cannot open %s\n", a < 0 ? argv[1] : argv[2]);
    return 2;
  }
  for(;;){
    n = read(a, x, sizeof(x));
    m = read(b, y, sizeof(y));
    if(n < 0 || m < 0){
      printf(2, "cmp: read failed\n");
      return 2;
    }
    for(i = 0; i < n && i < m && x[i] == y[i]; i++)
      ;
    if(i < n && i < m){
      printf(1, "%s %s differ: byte %d\n", argv[1], argv[2], offset+i+1);
      return 1;
    }
    if(n != m){
      printf(1, "cmp: EOF on %s after byte %d\n",
             n < m ? argv[1] : argv[2], offset+i);
      return 1;
    }
    if(n == 0)
      break;
    offset += n;
  }
  return 0;
}
