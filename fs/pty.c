/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "stat.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "mode.h"
#include "p9abi.h"
#include "tty.h"
#include "ioctl.h"

enum { in_capacity = 512, out_capacity = 2048, end_of_input = 256 };

struct pty {
  struct spinlock lock;
  uint ir, iw, edit, or, ow;
  short in[in_capacity];
  char out[out_capacity];
  int master, slaves, opened, owner;
  int sid, fg;
  struct termios term;
  struct winsize size;
};

static struct pty terminals[P9_NPTY];
static const struct termios defaults = TTY_DEFAULTS;

void
ptyinit(void)
{
  for(int i = 0; i < P9_NPTY; i++)
    initlock(&terminals[i].lock, "pty");
}

static void
attachfile(struct file *f, struct pty *p, int master)
{
  f->type = FD_PTY;
  f->readable = f->writable = 1;
  f->pty = p;
  f->ptymaster = master;
}

int
ptyopen(struct file *f, int minor, int master, int nonblock, int readonly, int writeonly)
{
  struct pty *p;
  if(minor < 0 || minor >= P9_NPTY)
    return -P9_ENODEV;
  p = &terminals[minor];
  acquire(&p->lock);
  if(master){
    if(p->master || p->slaves){
      release(&p->lock);
      return -P9_EBUSY;
    }
    p->ir = p->iw = p->edit = p->or = p->ow = 0;
    p->owner = myproc()->uid;
    p->term = defaults;
    p->term.c_oflag = OPOST | ONLCR;
    p->size = (struct winsize){ TTY_ROWS, TTY_COLS, 0, 0 };
    p->master = 1;
    p->opened = p->sid = p->fg = 0;
  } else {
    if(!p->master || (myproc()->uid && p->owner != myproc()->uid)){
      release(&p->lock);
      return -P9_EACCES;
    }
    p->slaves++;
    p->opened = 1;
  }
  attachfile(f, p, master);
  f->nonblock = nonblock;
  f->readable = !writeonly;
  f->writable = !readonly;
  release(&p->lock);
  return 0;
}

int
ptyalloc(struct file *master, struct file *slave)
{
  for(int i = 0; i < P9_NPTY; i++){
    if(ptyopen(master, i, 1, 0, 0, 0) >= 0){
      // the new master holds the slot until the slave descriptor is installed
      if(ptyopen(slave, i, 0, 0, 0, 0) < 0)
        panic("ptyalloc");
      return i;
    }
  }
  return -P9_EAGAIN;
}

void
ptyclose(struct pty *p, int master)
{
  acquire(&p->lock);
  if(master){
    p->master = 0;
    p->ir = p->iw = p->edit;
    ptysignal(p, SIGHUP);
  } else {
    p->slaves--;
  }
  wakeup(p);
  selectnotify();
  release(&p->lock);
}

int *
ptyforeground(struct pty *p)
{
  return &p->fg;
}

int
ptyowner(struct pty *p)
{
  return p->owner;
}

int
ptymode(struct file *f, int request)
{
  if(request == TTY_ISATTY)
    return 0;
  if(f->ptymaster || !f->readable ||
     (request != TTY_CBREAK && request != TTY_CANON))
    return -1;
  struct pty *p = f->pty;
  acquire(&p->lock);
  int result = ttysetcbreak(p, request == TTY_CBREAK);
  if(result == 0 && request == TTY_CBREAK){
    p->iw = p->edit;
    wakeup(p);
    selectnotify();
  }
  release(&p->lock);
  return result;
}

int
ptymetadata(struct file *f, struct stat *s)
{
  memset(s, 0, sizeof(*s));
  s->type = T_DEV;
  s->ino = (f->pty - terminals) * 2 + 1 + f->ptymaster;
  s->nlink = 1;
  return 0;
}

int
ptyioctl(struct file *f, int request, void *data)
{
  struct pty *p = f->pty;
  int result = 0;
  acquire(&p->lock);
  switch(request){
  case TIOCSCTTY:
    if(f->ptymaster || !p->master || p->sid || ptyassign(f) < 0){
      result = -P9_EPERM;
    } else {
      p->sid = myproc()->sid;
    }
    break;
  case TIOCGPTY:
    *(int*)data = (p - terminals) * 2 + f->ptymaster;
    break;
  case TIOCGWINSZ:
    *(struct winsize*)data = p->size;
    break;
  case TIOCSWINSZ: {
    struct winsize next = *(struct winsize*)data;
    if(!next.ws_col || !next.ws_row)
      result = -P9_EINVAL;
    else if(memcmp(&p->size, &next, sizeof(next))){
      p->size = next;
      ptysignal(p, SIGWINCH);
    }
    break;
  }
  case TIOCGETA:
    *(struct termios*)data = p->term;
    break;
  case TIOCSETA:
  case TIOCSETAW:
  case TIOCSETAF: {
    const struct termios *next = data;
    if(next->c_cc[VTIME] || (next->c_oflag & ~(OPOST | ONLCR))){
      result = -P9_EINVAL;
      break;
    }
    if(!(next->c_lflag & ICANON)){
      // bytes buffered by canonical mode can now be read directly
      p->iw = p->edit;
    }
    if(request == TIOCSETAF)
      p->ir = p->iw = p->edit;
    p->term = *next;
    wakeup(p);
    selectnotify();
    break;
  }
  default:
    result = -P9_ENOTTY;
  }
  release(&p->lock);
  return result;
}

static int
available(struct pty *p, int master)
{
  if(master)
    return p->ow != p->or || (!p->slaves && p->opened);
  uint n = p->iw - p->ir;
  if(ttyinputowner(p))
    return !p->master || n != 0;
  return !p->master || (p->term.c_lflag & ICANON ? n != 0 :
                        n >= p->term.c_cc[VMIN] || n == in_capacity);
}

int
ptyready(struct pty *p, int master, int writing)
{
  acquire(&p->lock);
  int result = writing ?
    (master ? p->edit - p->ir < in_capacity : !p->master || p->ow - p->or < out_capacity) :
    available(p, master);
  release(&p->lock);
  return result;
}

int
ptyread(struct pty *p, int master, char *data, int size, int nonblock)
{
  int n = 0;
  acquire(&p->lock);
  for(;;){
    int restricted = master ? 0 : ptyreadcheck(p);
    if(restricted < 0){
      release(&p->lock);
      return restricted;
    }
    if(available(p, master) || (!master && !(p->term.c_lflag & ICANON) && p->iw - p->ir >= size))
      break;
    if(nonblock || sleepintr(p, &p->lock) < 0){
      release(&p->lock);
      return nonblock ? -P9_EAGAIN : -P9_EINTR;
    }
  }
  if(master){
    while(n < size && p->or != p->ow)
      data[n++] = p->out[p->or++ % out_capacity];
  } else {
    while(n < size && p->ir != p->iw){
      int c = p->in[p->ir % in_capacity];
      if(c == end_of_input && n)
        break;
      p->ir++;
      if(c == end_of_input)
        break;
      data[n++] = c;
      if((p->term.c_lflag & ICANON) && c == '\n' && !ttyinputowner(p))
        break;
    }
  }
  wakeup(p);
  selectnotify();
  release(&p->lock);
  return n;
}

static int
outputwidth(struct pty *p, int c)
{
  return c == '\n' && (p->term.c_oflag & (OPOST | ONLCR)) == (OPOST | ONLCR) ? 2 : 1;
}

static void
output(struct pty *p, int c)
{
  int width = outputwidth(p, c);
  if(out_capacity - (p->ow - p->or) < width)
    return;
  if(width == 2)
    p->out[p->ow++ % out_capacity] = '\r';
  p->out[p->ow++ % out_capacity] = c;
}

static int
iscontrol(struct pty *p, int c, int index)
{
  return p->term.c_cc[index] && c == p->term.c_cc[index];
}

static void
ptyinput(struct pty *p, int c)
{
  int echo = p->term.c_lflag & ECHO;
  int canonical = p->term.c_lflag & ICANON;
  if(ttyinputowner(p))
    echo = canonical = 0;
  if(c == '\r' && (p->term.c_iflag & ICRNL))
    c = '\n';
  if(p->term.c_lflag & ISIG){
    int sig = iscontrol(p, c, VINTR) ? SIGINT : iscontrol(p, c, VQUIT) ? SIGQUIT :
              iscontrol(p, c, VSUSP) ? SIGTSTP : 0;
    if(sig){
      p->ir = p->iw = p->edit;
      if(echo){
        output(p, '^');
        output(p, c ^ 0x40);
        output(p, '\n');
      }
      ptysignal(p, sig);
      return;
    }
  }
  if(canonical && (iscontrol(p, c, VERASE) || c == 127 || iscontrol(p, c, VKILL))){
    int kill_line = iscontrol(p, c, VKILL);
    while(p->edit != p->iw){
      p->edit--;
      if(echo){
        output(p, '\b');
        output(p, ' ');
        output(p, '\b');
      }
      if(!kill_line)
        break;
    }
    return;
  }
  if(canonical && iscontrol(p, c, VEOF))
    c = end_of_input;
  else if(echo)
    output(p, c);
  p->in[p->edit++ % in_capacity] = c;
  if(!canonical || c == '\n' || c == end_of_input || p->edit - p->ir == in_capacity)
    p->iw = p->edit;
}

int
ptywrite(struct pty *p, int master, const char *data, int size, int nonblock)
{
  int n = 0, error = 0;
  acquire(&p->lock);
  while(n < size){
    if((!master && !p->master) || (master && !p->slaves && p->opened)){
      error = -P9_EIO;
      break;
    }
    int full = master ? p->edit - p->ir == in_capacity :
      out_capacity - (p->ow - p->or) < outputwidth(p, (uchar)data[n]);
    if(full){
      if(nonblock || sleepintr(p, &p->lock) < 0){
        error = nonblock ? -P9_EAGAIN : -P9_EINTR;
        break;
      }
      continue;
    }
    if(master)
      ptyinput(p, (uchar)data[n++]);
    else
      output(p, (uchar)data[n++]);
    wakeup(p);
    selectnotify();
  }
  release(&p->lock);
  return n ? n : error;
}

int
ptyevents(struct pty *p, int master)
{
  acquire(&p->lock);
  int result = (master ? !p->slaves && p->opened : !p->master) ? POLLHUP : 0;
  release(&p->lock);
  return result;
}
