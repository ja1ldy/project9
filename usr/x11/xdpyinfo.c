/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stdio.h>
#include <unistd.h>
#include "xclient.h"

int
main(void)
{
  struct display d;
  if(xopen(&d) < 0){
    perror("xdpyinfo: cannot open display");
    return 1;
  }
  printf("name of display: :0\nversion number: 11.0\n"
         "root window id: 0x%x\ndimensions: %ux%u pixels\ndepth of root window: %u planes\n",
         d.root.windowId, d.root.pixWidth, d.root.pixHeight, d.root.rootDepth);
  close(d.fd);
  return 0;
}
