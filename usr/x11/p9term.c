/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include "xclient.h"

enum { MaxRows = 32, MaxCols = 110, CharWidth = 6, CharHeight = 13, Margin = 10 };
enum { Ground, Escape, Csi, String, StringEscape };
struct cell { unsigned char ch, fg, bg; };
struct terminal {
  struct cell cells[MaxRows][MaxCols];
  struct cell shown[MaxRows][MaxCols];
  unsigned char dirty[MaxRows];
  int row, col, rows, cols, fg, bg, state, savedrow, savedcol, cursor;
  int param[8], count, private;
};
static struct terminal term;
static const uint32_t rgb[] = {
  0x151b22, 0xbd423f, 0x479260, 0xcda757, 0x6588c1, 0x9a6eb1, 0x519b9c, 0xd8e2ea,
  0x657381, 0xfb736b, 0x77ce92, 0xf9d67c, 0x8cb7ff, 0xdbaaee, 0x72dcda, 0xffffff
};
struct window {
  struct display d;
  uint32_t id, gc;
  uint32_t *keymap;
  unsigned per_key;
};

static int
limit(int n, int max)
{
  return n < 0 ? 0 : n >= max ? max - 1 : n;
}

static void
erase(int row, int from, int to)
{
  for(int i = from; i < to; i++)
    term.cells[row][i] = (struct cell){ ' ', term.fg, term.bg };
  term.dirty[row] = 1;
}

static void
newline(void)
{
  if(term.row < term.rows - 1){
    ++term.row;
  } else {
    memmove(term.cells, term.cells + 1, (term.rows - 1) * sizeof(term.cells[0]));
    memset(term.dirty, 1, term.rows);
    erase(term.rows - 1, 0, term.cols);
  }
}

static void
reset(void)
{
  term.bg = 0;
  term.fg = 7;
  term.cursor = 1;
  term.row = term.col = 0;
  for(int i = 0; i < term.rows; i++)
    erase(i, 0, term.cols);
}

static int
param(int i, int fallback)
{
  return i < term.count && term.param[i] ? term.param[i] : fallback;
}

static void
csi(int code)
{
  int n = param(0, 1);
  if(term.private){
    if(term.private == '?' && term.param[0] == 25 && (code == 'h' || code == 'l'))
      term.cursor = code == 'h';
    return;
  }
  switch(code){
  case 'A': term.row = limit(term.row - n, term.rows); break;
  case 'B': term.row = limit(term.row + n, term.rows); break;
  case 'C': term.col = limit(term.col + n, term.cols); break;
  case 'D': term.col = limit(term.col - n, term.cols); break;
  case 'G': term.col = limit(n-1, term.cols); break;
  case 'd': term.row = limit(n-1, term.rows); break;
  case 'H': case 'f':
    term.row = limit(n-1, term.rows);
    term.col = limit(param(1, 1)-1, term.cols);
    break;
  case 'J':
    if(term.param[0] == 2){
      for(int i = 0; i < term.rows; i++)
        erase(i, 0, term.cols);
    } else if(term.param[0] == 0){
      erase(term.row, term.col, term.cols);
      for(int i = term.row + 1; i < term.rows; i++)
        erase(i, 0, term.cols);
    } else if(term.param[0] == 1){
      for(int i = 0; i < term.row; i++)
        erase(i, 0, term.cols);
      erase(term.row, 0, limit(term.col, term.cols)+1);
    }
    break;
  case 'K':
    if(term.param[0] == 0) erase(term.row, term.col, term.cols);
    if(term.param[0] == 1) erase(term.row, 0, limit(term.col, term.cols)+1);
    if(term.param[0] == 2) erase(term.row, 0, term.cols);
    break;
  case 'm':
    for(int i = 0; i < term.count; i++){
      int a = term.param[i];
      if(!a){ term.fg = 7; term.bg = 0; }
      else if(a == 1) term.fg |= 8;
      else if(a == 22) term.fg &= 7;
      else if(a == 7){ int swap = term.fg; term.fg = term.bg; term.bg = swap; }
      else if(a == 39) term.fg = 7;
      else if(a == 49) term.bg = 0;
      else if(a >= 30 && a <= 37) term.fg = a - 30;
      else if(a >= 40 && a <= 47) term.bg = a - 40;
      else if(a >= 90 && a <= 97) term.fg = a - 90 + 8;
      else if(a >= 100 && a <= 107) term.bg = a - 100 + 8;
    }
    break;
  case 's': term.savedrow = term.row; term.savedcol = term.col; break;
  case 'u': term.row = term.savedrow; term.col = term.savedcol; break;
  }
}

static void
consume(unsigned c)
{
  switch(term.state){
  case Escape:
    term.state = Ground;
    if(c == '['){
      memset(term.param, 0, sizeof(term.param));
      term.count = 1;
      term.private = 0;
      term.state = Csi;
    } else if(c == ']' || c == 'P'){
      term.state = String;
    } else if(c == 'c'){
      reset();
    }
    return;
  case String:
    if(c == 7) term.state = Ground;
    if(c == 27) term.state = StringEscape;
    return;
  case StringEscape:
    term.state = c == '\\' ? Ground : String;
    return;
  case Csi:
    if(c >= '0' && c <= '9'){
      int *p = &term.param[term.count-1];
      if(*p < 1000) *p = *p * 10 + c - '0';
    } else if(c == ';'){
      if(term.count < sizeof(term.param)/sizeof(*term.param)) term.count++;
      else term.private = 1;
    } else if(c >= 0x40 && c <= 0x7e){
      csi(c);
      term.state = Ground;
    } else {
      term.private = c;
    }
    return;
  }
  if(c == 27) term.state = Escape;
  else if(c == '\f'){
    term.row = term.col = 0;
    for(int i = 0; i < term.rows; i++)
      erase(i, 0, term.cols);
  }
  else if(c == '\r') term.col = 0;
  else if(c == '\n'){ term.col = 0; newline(); }
  else if(c == '\b' && term.col > 0) term.col--;
  else if(c == '\t') term.col = limit((term.col + 8) & ~7, term.cols);
  else if(c >= 32 && c < 127){
    if(term.col >= term.cols){
      term.col = 0;
      newline();
    }
    term.cells[term.row][term.col++] = (struct cell){ c, term.fg, term.bg };
    term.dirty[term.row] = 1;
  }
}

static int
samecell(struct cell a, struct cell b)
{
  return a.ch == b.ch && a.fg == b.fg && a.bg == b.bg;
}

static struct cell
displaycell(int row, int col, int cursor)
{
  struct cell result = term.cells[row][col];
  if(col == cursor){
    unsigned char fg = result.fg;
    result.fg = result.bg;
    result.bg = fg;
  }
  return result;
}

static int
repaint(struct window *w)
{
  for(int row = 0; row < term.rows; row++){
    if(!term.dirty[row])
      continue;
    term.dirty[row] = 0;
    int cursor = term.cursor && row == term.row ? limit(term.col, term.cols) : -1;
    struct cell *shown = term.shown[row];
    for(int col = 0; col < term.cols;){
      struct cell first = displaycell(row, col, cursor);
      if(samecell(first, shown[col])){
        col++;
        continue;
      }
      int from = col;
      struct cell next = first;
      char text[MaxCols + 4] = {0};
      for(;;){
        text[col - from] = next.ch;
        shown[col++] = next;
        if(col == term.cols)
          break;
        next = displaycell(row, col, cursor);
        if(next.fg != first.fg || next.bg != first.bg || samecell(next, shown[col]))
          break;
      }
      XREQ(xChangeGCReq, gc, ChangeGC);
      gc.mask = GCForeground | GCBackground;
      gc.gc = w->gc;
      gc.length += 2;
      uint32_t colors[] = {rgb[first.fg], rgb[first.bg]};
      XREQ(xImageText8Req, draw, ImageText8);
      draw.length += XPAD(col - from)/4;
      draw.nChars = col - from;
      draw.drawable = w->id;
      draw.gc = w->gc;
      draw.x = Margin + from * CharWidth;
      draw.y = Margin + 11 + row * CharHeight;
      if(xrequest(&w->d, &gc, sizeof(gc)) || xwrite(&w->d, colors, sizeof(colors)) ||
         xrequest(&w->d, &draw, sizeof(draw)) || xwrite(&w->d, text, XPAD(col - from)))
        return -1;
    }
  }
  return 0;
}

static int
createwindow(struct window *w)
{
  struct display *d = &w->d;
  w->id = d->setup.ridBase | 1;
  w->gc = d->setup.ridBase | 2;
  uint32_t fontid = d->setup.ridBase | 3;
  int width = term.cols * CharWidth + 2 * Margin, height = term.rows * CharHeight + 2 * Margin;
  XREQ(xCreateWindowReq, create, CreateWindow);
  create.wid = w->id;
  create.parent = d->root.windowId;
  create.class = InputOutput;
  create.x = (d->root.pixWidth - width)/2;
  create.y = (d->root.pixHeight - height)/2;
  create.width = width;
  create.height = height;
  create.mask = CWBackPixel | CWEventMask;
  create.length += 2;
  uint32_t attrs[] = {
    rgb[0], ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask
  };
  XREQ(xChangePropertyReq, title, ChangeProperty);
  static const char caption[16] = "Project9 shell";
  static const char classname[16] = "p9term\0P9term";
  title.window = w->id;
  title.property = XA_WM_NAME;
  title.type = XA_STRING;
  title.format = 8;
  title.nUnits = sizeof(caption)-2;
  title.length += sizeof(caption)/4;
  xChangePropertyReq icon = title, class = title;
  icon.property = XA_WM_ICON_NAME;
  class.property = XA_WM_CLASS;
  class.nUnits = sizeof("p9term\0P9term");
  XREQ(xOpenFontReq, font, OpenFont);
  font.fid = fontid;
  font.nbytes = 5;
  font.length += 2;
  XREQ(xCreateGCReq, gc, CreateGC);
  gc.gc = w->gc;
  gc.drawable = w->id;
  gc.mask = GCFont;
  gc.length++;
  XREQ(xResourceReq, map, MapWindow);
  map.id = w->id;
  return xrequest(d, &create, sizeof(create)) || xwrite(d, attrs, sizeof(attrs)) ||
    xrequest(d, &title, sizeof(title)) || xwrite(d, caption, sizeof(caption)) ||
    xrequest(d, &icon, sizeof(icon)) || xwrite(d, caption, sizeof(caption)) ||
    xrequest(d, &class, sizeof(class)) || xwrite(d, classname, sizeof(classname)) ||
    xrequest(d, &font, sizeof(font)) || xwrite(d, "fixed\0\0\0", 8) ||
    xrequest(d, &gc, sizeof(gc)) || xwrite(d, &fontid, sizeof(fontid)) ||
    xsync(d) || xrequest(d, &map, sizeof(map)) ? -1 : 0;
}

static int
keymap(struct window *w)
{
  struct display *d = &w->d;
  XREQ(xGetKeyboardMappingReq, req, GetKeyboardMapping);
  req.firstKeyCode = d->setup.minKeyCode;
  if(d->setup.maxKeyCode < req.firstKeyCode || !req.firstKeyCode)
    return -1;
  req.count = d->setup.maxKeyCode - req.firstKeyCode + 1;
  xGetKeyboardMappingReply reply;
  if(xrequest(d, &req, sizeof(req)) || xreply(d, &reply) ||
     !reply.keySymsPerKeyCode || reply.length != req.count * reply.keySymsPerKeyCode)
    return -1;
  uint32_t *keys = malloc(reply.length * sizeof(*keys));
  if(!keys)
    return -1;
  if(xread(d, keys, reply.length * sizeof(*keys)) < 0){
    free(keys);
    return -1;
  }
  free(w->keymap);
  w->keymap = keys;
  w->per_key = reply.keySymsPerKeyCode;
  return 0;
}

static int
keyboard(struct window *w, xEvent *e, int master)
{
  unsigned raw = e->u.u.detail;
  if(raw < w->d.setup.minKeyCode || raw > w->d.setup.maxKeyCode)
    return 0;
  uint32_t *keys = w->keymap + (raw - w->d.setup.minKeyCode) * w->per_key;
  unsigned flags = e->u.keyButtonPointer.state;
  uint32_t key = (flags & ShiftMask) && w->per_key > 1 && keys[1] ? keys[1] : keys[0];
  char one;
  const char *text = &one;
  size_t bytes = 1;
  switch(key){
  case XK_Return: case XK_KP_Enter: one = '\r'; break;
  case XK_BackSpace: one = '\b'; break;
  case XK_Tab: one = '\t'; break;
  case XK_Escape: one = 27; break;
  case XK_Up: text = "\033[A"; bytes = 3; break;
  case XK_Down: text = "\033[B"; bytes = 3; break;
  case XK_Right: text = "\033[C"; bytes = 3; break;
  case XK_Left: text = "\033[D"; bytes = 3; break;
  case XK_Home: text = "\033[H"; bytes = 3; break;
  case XK_End: text = "\033[F"; bytes = 3; break;
  case XK_Delete: text = "\033[3~"; bytes = 4; break;
  case XK_Prior: text = "\033[5~"; bytes = 4; break;
  case XK_Next: text = "\033[6~"; bytes = 4; break;
  default:
    if(key < 32 || key > 126)
      return 0;
    if((flags & LockMask) && ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')))
      key ^= 32;
    one = flags & ControlMask ? key & 31 : key;
  }
  return write(master, text, bytes) == (ssize_t)bytes ? 0 : -1;
}

static pid_t
shell(int master, int slave, int xfd, char **command)
{
  pid_t child = fork();
  if(!child){
    close(master);
    close(xfd);
    if(setsid() < 0 || ioctl(slave, TIOCSCTTY) < 0)
      _exit(126);
    for(int i = 0; i < 3; i++)
      if(dup2(slave, i) < 0)
        _exit(126);
    if(slave > 2) close(slave);
    if(setenv("TERM", "vt100", 1) < 0)
      _exit(126);
    execve(command[0], command, environ);
    perror(command[0]);
    _exit(127);
  }
  return child;
}

static int
events(struct window *w, int master)
{
  for(;;){
    struct pollfd ready[] = {{w->d.fd, POLLIN, 0}, {master, POLLIN, 0}};
    int count = poll(ready, 2, -1);
    if(count < 0 && errno == EINTR)
      continue;
    if(count < 0 || (ready[0].revents & (POLLHUP | POLLERR | POLLNVAL)))
      return 1;
    if(ready[0].revents & POLLIN){
      xEvent event;
      if(xread(&w->d, &event, sizeof(event)) < 0 || !event.u.u.type)
        return 1;
      if(event.u.u.type == KeyPress && keyboard(w, &event, master) < 0)
        return 1;
      if(event.u.u.type == ButtonPress || event.u.u.type == MapNotify){
        XREQ(xSetInputFocusReq, focus, SetInputFocus);
        focus.focus = w->id;
        focus.revertTo = RevertToParent;
        if(xrequest(&w->d, &focus, sizeof(focus)) < 0)
          return 1;
      }
      if(event.u.u.type == MappingNotify && keymap(w) < 0)
        return 1;
      if(event.u.u.type == Expose){
        memset(term.shown, 0, sizeof(term.shown));
        memset(term.dirty, 1, term.rows);
      }
    }
    if(ready[1].revents & (POLLIN | POLLHUP)){
      unsigned char bytes[1024];
      int got = read(master, bytes, sizeof(bytes));
      if(got <= 0)
        return got < 0 ? 1 : 0;
      term.dirty[term.row] = 1;
      for(int i = 0; i < got; i++)
        consume(bytes[i]);
      term.dirty[term.row] = 1;
    }
    if(repaint(w) < 0)
      return 1;
  }
}

int
main(int argc, char **argv)
{
  char *command[] = {"/bin/sh", NULL};
  if(argc > 1 && argv[1][0] != '/'){
    fputs("usage: p9term [/path/to/program [argument ...]]\n", stderr);
    return 2;
  }
  struct window w = {0};
  if(xopen(&w.d) < 0){
    perror("p9term: cannot open DISPLAY");
    return 1;
  }
  term.cols = (w.d.root.pixWidth - 2 * Margin) / CharWidth;
  term.rows = (w.d.root.pixHeight - 2 * Margin) / CharHeight;
  term.cols = term.cols < 80 ? term.cols : 80;
  term.rows = term.rows < 24 ? term.rows : 24;
  int master = -1, slave = -1, result = 1;
  pid_t pid = -1;
  struct winsize size = {term.rows, term.cols, term.cols * CharWidth, term.rows * CharHeight};
  if(term.cols < 10 || term.rows < 5 || keymap(&w) < 0 ||
     createwindow(&w) < 0 || openpty(&master, &slave, NULL, NULL, &size) < 0){
    fputs("p9term: cannot create a window or terminal\n", stderr);
    goto done;
  }
  reset();
  pid = shell(master, slave, w.d.fd, argc > 1 ? argv + 1 : command);
  close(slave);
  slave = -1;
  if(pid >= 0)
    result = events(&w, master);

done:
  close(w.d.fd);
  if(master >= 0) close(master);
  if(slave >= 0) close(slave);
  free(w.keymap);
  if(pid > 0)
    while(waitpid(pid, NULL, 0) < 0 && errno == EINTR){}
  return result;
}
