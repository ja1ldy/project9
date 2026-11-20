/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/extensions/randrproto.h>
#include "xclient.h"

int
main(int argc, char **argv)
{
  if(argc > 2 || (argc == 2 && strcmp(argv[1], "--query"))){
    fputs("usage: xrandr [--query]\n", stderr);
    return 2;
  }
  struct display d;
  if(xopen(&d) < 0){
    perror("xrandr: cannot open display");
    return 1;
  }
  XREQ(xQueryExtensionReq, query, QueryExtension);
  query.nbytes = 5;
  query.length += 2;
  xQueryExtensionReply ext;
  int failed = xrequest(&d, &query, sizeof(query)) || xwrite(&d, "RANDR\0\0\0", 8) ||
    xreply(&d, &ext) || !ext.present;
  if(failed){
    fputs("xrandr: server has no RandR extension\n", stderr);
    goto done;
  }
  xRRQueryVersionReq ver = { .reqType = ext.major_opcode, .randrReqType = X_RRQueryVersion,
     .length = sizeof(ver)/4, .majorVersion = 1, .minorVersion = 2 };
  xRRQueryVersionReply reply;
  xRRGetScreenInfoReq req = { .reqType = ext.major_opcode, .randrReqType = X_RRGetScreenInfo,
     .length = sizeof(req)/4, .window = d.root.windowId };
  xRRGetScreenInfoReply info;
  failed = xrequest(&d, &ver, sizeof(ver)) || xreply(&d, &reply) ||
    xrequest(&d, &req, sizeof(req)) || xreply(&d, &info) ||
    info.length < info.nSizes * sizeof(xScreenSizes)/4;
  if(!failed)
    printf("RandR %u.%u\nScreen 0: current %u x %u\n", reply.majorVersion, reply.minorVersion,
           d.root.pixWidth, d.root.pixHeight);
  for(unsigned i = 0; !failed && i < info.nSizes; i++){
    xScreenSizes dim;
    failed = xread(&d, &dim, sizeof(dim));
    if(!failed)
      printf("  %ux%u%s\n", dim.widthInPixels, dim.heightInPixels, i == info.sizeID ? " *" : "");
  }
done:
  close(d.fd);
  return !!failed;
}
