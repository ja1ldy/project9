/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "xclient.h"

int
main(int argc, char **argv)
{
  uint32_t id = 0;
  int tree = 0;
  for(int i = 1; i < argc; i++){
    if(!strcmp(argv[i], "-root"))
      id = 0;
    else if(!strcmp(argv[i], "-tree"))
      tree = 1;
    else if(!strcmp(argv[i], "-id") && i + 1 < argc && xid(argv[++i], &id) == 0)
      continue;
    else {
      fputs("usage: xwininfo [-root | -id window] [-tree]\n", stderr);
      return 2;
    }
  }
  struct display d;
  if(xopen(&d) < 0){
    perror("xwininfo: cannot open display");
    return 1;
  }
  if(!id)
    id = d.root.windowId;
  XREQ(xResourceReq, geom, GetGeometry);
  geom.id = id;
  xGetGeometryReply info;
  int failed = xrequest(&d, &geom, sizeof(geom)) || xreply(&d, &info);
  if(!failed)
    printf("xwininfo: Window id: 0x%x\n  Position: %d, %d\n  Width: %u\n  Height: %u\n  Depth: %u\n",
           id, info.x, info.y, info.width, info.height, info.depth);
  if(!failed && tree){
    XREQ(xResourceReq, query, QueryTree);
    query.id = id;
    xQueryTreeReply result;
    failed = xrequest(&d, &query, sizeof(query)) || xreply(&d, &result) ||
             result.length != result.nChildren;
    if(!failed)
      printf("  Root: 0x%x  Parent: 0x%x\n  Children: %u\n", result.root, result.parent, result.nChildren);
    for(unsigned i = 0; !failed && i < result.nChildren; i++){
      uint32_t child;
      failed = xread(&d, &child, sizeof(child));
      if(!failed)
        printf("    0x%x\n", child);
    }
  }
  if(failed)
    fputs("xwininfo: X server could not query the window\n", stderr);
  close(d.fd);
  return !!failed;
}
