/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "xclient.h"

int
main(int argc, char **argv)
{
  uint32_t id = 0;
  if(argc != 3 || strcmp(argv[1], "-id") || xid(argv[2], &id) < 0){
    fputs("usage: xkill -id window\n", stderr);
    return 2;
  }
  struct display d;
  if(xopen(&d) < 0){
    perror("xkill: cannot open display");
    return 1;
  }
  XREQ(xResourceReq, req, KillClient);
  req.id = id;
  int failed = xrequest(&d, &req, sizeof(req)) || xsync(&d);
  if(failed)
    fputs("xkill: the X server rejected the request\n", stderr);
  close(d.fd);
  return !!failed;
}
