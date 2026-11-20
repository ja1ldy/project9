/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include "xclient.h"

static int
read_property(struct display *d, uint32_t win, uint32_t prop, const char *name, int show_missing)
{
  unsigned char bytes[512];
  XREQ(xGetPropertyReq, request, GetProperty);
  request.window = win;
  request.property = prop;
  request.type = AnyPropertyType;
  request.longLength = sizeof(bytes)/4;
  xGetPropertyReply reply;
  if(xrequest(d, &request, sizeof(request)) || xreply(d, &reply) ||
     reply.length > sizeof(bytes)/4 || xread(d, bytes, reply.length*4) < 0)
    return -1;
  if(!reply.propertyType){
    if(show_missing)
      printf("%s: not found\n", name);
  } else if(reply.format == 8 && reply.nItems <= reply.length*4){
    printf("%s = \"", name);
    for(unsigned i = 0; i < reply.nItems; i++){
      unsigned c = bytes[i];
      if(!c && i+1 == reply.nItems)
        break;
      if(c == 0)
        fputs("\\0", stdout);
      else if(c < 32 || c > 126)
        printf("\\x%02x", c);
      else {
        if(c == '"' || c == '\\') putchar('\\');
        putchar(c);
      }
    }
    puts(reply.bytesAfter ? "\" (truncated)" : "\"");
  } else {
    printf("%s: %u items of %u bits\n", name, reply.nItems, reply.format);
  }
  return 0;
}

int
main(int argc, char **argv)
{
  uint32_t id = 0;
  int i = 1, root = 0;
  if(argc > i && !strcmp(argv[i], "-root")){
    root = 1;
    ++i;
  } else if(argc > i && !strcmp(argv[i], "-id")){
    if(argc <= i+1 || xid(argv[i+1], &id) < 0)
      return 2;
    i += 2;
  }
  static const struct { const char *name; uint32_t atom; } properties[] = {
    {"WM_NAME", XA_WM_NAME}, {"WM_ICON_NAME", XA_WM_ICON_NAME}, {"WM_CLASS", XA_WM_CLASS}
  };
  int which = -1;
  if(argc > i){
    if(i+1 == argc)
      for(unsigned n = 0; n < sizeof(properties)/sizeof(properties[0]); n++)
        if(!strcmp(argv[i], properties[n].name))
          which = n;
    if(which < 0){
      fputs("usage: xprop [-root | -id window] [WM_NAME | WM_ICON_NAME | WM_CLASS]\n", stderr);
      return 2;
    }
  }
  struct display d;
  if(xopen(&d) < 0){
    perror("xprop: cannot open display");
    return 1;
  }
  if(!id && !root){
    XREQ(xReq, focus, GetInputFocus);
    xGetInputFocusReply reply;
    if(xrequest(&d, &focus, sizeof(focus)) || xreply(&d, &reply)){
      close(d.fd);
      return 1;
    }
    if(reply.focus > PointerRoot)
      id = reply.focus;
  }
  if(!id)
    id = d.root.windowId;
  int result = 0;
  for(unsigned n = 0; n < sizeof(properties)/sizeof(properties[0]); n++)
    if((which < 0 || (unsigned)which == n) &&
       read_property(&d, id, properties[n].atom, properties[n].name, which >= 0) < 0){
      result = 1;
      break;
    }
  close(d.fd);
  return result;
}
