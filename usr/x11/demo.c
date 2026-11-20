/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/Xproto.h>
#include "demo.h"

#define REQ(type, name, opcode) type name = { .reqType = X_ ## opcode, .length = sizeof(type)/4 }
#define PAD(n) (((n) + 3U) & ~3U)
#define PANEL 0x214762U
#define LIGHT 0xe4f4ffU
#define ACCENT 0x50d6adU

struct view {
  uint32_t window, gc;
  unsigned width, height;
};

static int
exchange(int fd, void *data, size_t size)
{
  unsigned char *p = data;
  while(size){
    ssize_t n = fd == 0 ? read(fd, p, size) : write(fd, p, size);
    if(n < 0 && errno == EINTR)
      continue;
    if(n <= 0)
      return -1;
    p += n;
    size -= n;
  }
  return 0;
}

static int
setup(xConnSetup *setup, xWindowRoot *root)
{
  xConnClientPrefix hello = { .byteOrder = 'l', .majorVersion = 11 };
  xConnSetupPrefix reply;
  unsigned char body[4096];
  if(exchange(1, &hello, sizeof(hello)) < 0 || exchange(0, &reply, sizeof(reply)) < 0 ||
     reply.success != 1 || reply.majorVersion != 11 || reply.length * 4U > sizeof(body) ||
     reply.length * 4U < sizeof(*setup) || exchange(0, body, reply.length * 4U) < 0)
    return -1;
  memcpy(setup, body, sizeof(*setup));
  unsigned root_at = sizeof(*setup) + PAD(setup->nbytesVendor) + setup->numFormats * sizeof(xPixmapFormat);
  if(setup->numRoots != 1 || root_at + sizeof(*root) > reply.length * 4U)
    return -1;
  memcpy(root, body + root_at, sizeof(*root));
  return 0;
}

static int
color(const struct view *view, uint32_t pixel)
{
  REQ(xChangeGCReq, req, ChangeGC);
  req.length++;
  req.gc = view->gc;
  req.mask = GCForeground;
  return exchange(1, &req, sizeof(req)) || exchange(1, &pixel, sizeof(pixel)) ? -1 : 0;
}

static int
text(const struct view *view, int x, int y, const char *line)
{
  REQ(xPolyText8Req, req, PolyText8);
  unsigned char item[80] = {0};
  size_t n = strlen(line);
  if(n + 2 > sizeof(item))
    return -1;
  req.length += PAD(n + 2)/4;
  req.drawable = view->window;
  req.gc = view->gc;
  req.x = x;
  req.y = y;
  item[0] = n;
  memcpy(item + 2, line, n);
  return exchange(1, &req, sizeof(req)) || exchange(1, item, PAD(n + 2)) ? -1 : 0;
}

static int
paint(const struct view *view, uint32_t accent)
{
  REQ(xPolyFillRectangleReq, req, PolyFillRectangle);
  xRectangle line = {16, 12, view->width - 32, 5};
  req.length += sizeof(line)/4;
  req.drawable = view->window;
  req.gc = view->gc;
  return (color(view, accent) < 0 || exchange(1, &req, sizeof(req)) < 0 ||
          exchange(1, &line, sizeof(line)) < 0 || color(view, LIGHT) < 0 ||
          text(view, 16, 39, "Project9 dev  |  Tiny-X") < 0 ||
          text(view, 16, 61, "Click to change color    Esc to exit") < 0) ? -1 : 0;
}

static int
verify_pixel(const struct view *view)
{
  REQ(xGetImageReq, req, GetImage);
  xEvent packet;
  xGetImageReply reply;
  uint32_t pixel;
  req.format = ZPixmap;
  req.drawable = view->window;
  req.x = 17;
  req.y = 13;
  req.width = req.height = 1;
  req.planeMask = 0xffffff;
  if(exchange(1, &req, sizeof(req)) < 0)
    return -1;
  for(;;){
    if(exchange(0, &packet, sizeof(packet)) < 0 || !packet.u.u.type)
      return -1;
    if(packet.u.u.type == X_Reply)
      break;
  }
  memcpy(&reply, &packet, sizeof(reply));
  return reply.length == 1 && reply.depth == 24 &&
         exchange(0, &pixel, sizeof(pixel)) == 0 && (pixel & 0xffffff) == ACCENT ? 0 : -1;
}

int
x11demo(int check)
{
  xConnSetup config;
  xWindowRoot root;
  if(setup(&config, &root) < 0 || root.pixWidth < 240 || root.pixHeight < 112 ||
     root.rootDepth != 24 || (config.ridMask & 7) != 7)
    goto fail;

  struct view view = { config.ridBase | 1, config.ridBase | 2,
                       root.pixWidth < 368 ? root.pixWidth - 16 : 352, 88 };
  REQ(xCreateWindowReq, window, CreateWindow);
  CARD32 window_values[] = {PANEL, ExposureMask | ButtonPressMask | KeyPressMask};
  window.length += sizeof(window_values)/4;
  window.wid = view.window;
  window.parent = root.windowId;
  window.x = (root.pixWidth - view.width)/2;
  window.y = (root.pixHeight - view.height)/2;
  window.width = view.width;
  window.height = view.height;
  window.class = InputOutput;
  window.mask = CWBackPixel | CWEventMask;

  REQ(xOpenFontReq, font, OpenFont);
  char fixed[8] = "fixed";
  font.fid = config.ridBase | 3;
  font.nbytes = 5;
  font.length += sizeof(fixed)/4;
  REQ(xCreateGCReq, gc, CreateGC);
  gc.length++;
  gc.gc = view.gc;
  gc.drawable = view.window;
  gc.mask = GCFont;
  REQ(xResourceReq, map, MapWindow);
  map.id = view.window;
  REQ(xSetInputFocusReq, focus, SetInputFocus);
  focus.focus = view.window;
  focus.revertTo = RevertToParent;
  if(exchange(1, &window, sizeof(window)) < 0 || exchange(1, window_values, sizeof(window_values)) < 0 ||
     exchange(1, &font, sizeof(font)) < 0 || exchange(1, fixed, sizeof(fixed)) < 0 ||
     exchange(1, &gc, sizeof(gc)) < 0 || exchange(1, &font.fid, sizeof(font.fid)) < 0 ||
     exchange(1, &map, sizeof(map)) < 0 || exchange(1, &focus, sizeof(focus)) < 0 ||
     paint(&view, ACCENT) < 0 || verify_pixel(&view) < 0)
    goto fail;
  if(check)
    return 0;

  uint32_t accent = ACCENT;
  for(;;){
    xEvent event;
    if(exchange(0, &event, sizeof(event)) < 0)
      return 0;
    if(event.u.u.type == KeyPress && event.u.u.detail == 9)
      return 0;
    if(event.u.u.type == ButtonPress || event.u.u.type == KeyPress)
      accent = accent == ACCENT ? 0xf5ac61 : ACCENT;
    if(event.u.u.type == Expose || event.u.u.type == ButtonPress || event.u.u.type == KeyPress)
      if(paint(&view, accent) < 0)
        return 0;
    if(!event.u.u.type)
      break;
  }

fail:
  fputs("tinyx: demo X11 request failed\n", stderr);
  return 1;
}
