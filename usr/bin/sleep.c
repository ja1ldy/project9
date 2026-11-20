/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "p9abi.h"

int
main(int argc, char **argv)
{
  uint count;
  int raw = argc == 3 && !strcmp(argv[1], "-t");
  uint scale = raw ? 1 : P9_HZ;
  uint round = !raw;

  if(argc != 2 + raw || parseuint(argv[1+raw], &count) < 0 ||
     count > (0x7fffffffU - round) / scale){
    printf(2, "usage: sleep seconds | sleep -t ticks\n");
    return 2;
  }
  if(!count)
    return 0;
  return sleep(count * scale + round) < 0;
}
