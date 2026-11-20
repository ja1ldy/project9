/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// the keyboard and serial port share one input line
// output goes to both the local display and the serial port

#include <stdarg.h>
#include "types.h"
#include "kassert.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "tty.h"
#include "ioctl.h"
#include "p9abi.h"
#include "keys.h"
#include "screen.h"

static void consputc(int);

static int panicked = 0;
static int echo_owner;
static int paused;
static int literal;
static const struct termios defaults = TTY_DEFAULTS;
static struct termios settings = TTY_DEFAULTS;
static struct winsize viewport = {TTY_ROWS, TTY_COLS, 0, 0};

static struct {
  struct spinlock lock;
  int locking;
  int framebuffer_users;
} cons;

static void
printint(uptr x, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[2 * sizeof(uptr) + 1];
  int i;

  if(sign)
    x = -x;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// print to the console with d x p and s
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  va_list args;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  va_start(args, fmt);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd': {
      int value = va_arg(args, int);
      printint((uptr)(long)value, 10, value < 0);
      break;
    }
    case 'x':
      printint(va_arg(args, uint), 16, 0);
      break;
    case 'p':
      printint((uptr)va_arg(args, void*), 16, 0);
      break;
    case 's':
      if((s = va_arg(args, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // leave unknown format codes visible in the output
      consputc('%');
      consputc(c);
      break;
    }
  }

  va_end(args);
  fbflush();
  if(locking)
    release(&cons.lock);
}

static void __attribute__((noreturn))
halt_with_trace(const char *message, const char *file, int line)
{
  uptr *frame, stack, next;
  int i;
  extern uint physlimit;
  uptr cr3;

  cli();
  asm volatile("mov %%cr3,%0" : "=r"(cr3));
  cons.locking = 0;
  cprintf("lapicid %d: panic: %s\n", lapicid(), message);
  if(file)
    cprintf("assertion at %s:%d\n", file, line);
  cprintf("cr3=0x%p memory-limit=0x%x\n", (void*)cr3, physlimit);
  frame = __builtin_frame_address(0);
  stack = PGROUNDDOWN((uptr)&cr3);
  cprintf("backtrace:");
  // stay inside this stack even if a saved frame pointer is damaged
  for(i = 0; i < 10; i++){
    if((uptr)frame < stack || (uptr)frame - stack > PGSIZE - 2 * sizeof(uptr) ||
       ((uptr)frame & (sizeof(uptr) - 1)) || frame[1] < KERNVIRT)
      break;
    cprintf(" %p", (void*)frame[1]);
    next = frame[0];
    if(next <= (uptr)frame)
      break;
    frame = (uptr*)next;
  }
  cprintf("\n");
  panicked = 1;
  for(;;)
    asm volatile("hlt");
}

void
panic(const char *message)
{
  halt_with_trace(message, 0, 0);
}

void
panictrap(struct trapframe *tf)
{
  cli();
  cons.locking = 0;
  cprintf("kernel trap %d err %d ip=0x%p cs=0x%x flags=0x%x\n",
          (int)tf->trapno, (int)tf->err, (void*)(uptr)tf->eip, (uint)tf->cs, (uint)tf->eflags);
  cprintf("eax=0x%x ebx=0x%x ecx=0x%x edx=0x%x ebp=0x%x\n",
          (uint)tf->eax, (uint)tf->ebx, (uint)tf->ecx, (uint)tf->edx, (uint)tf->ebp);
  if(tf->trapno == T_PGFLT)
    cprintf("fault address=0x%p\n", (void*)rcr2());
  halt_with_trace("unexpected kernel trap", 0, 0);
}

void
kassert_fail(const char *expr, const char *file, int line)
{
  halt_with_trace(expr, file, line);
}

//PAGEBREAK: 50
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);
static int cga_pos, cga_saved, cga_attr = 0x0700;

static void
cgacursor(void)
{
  outb(CRTPORT, 14);
  outb(CRTPORT+1, cga_pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, cga_pos);
}

static void
cgaerase(int first, int last)
{
  while(first < last)
    crt[first++] = ' ' | cga_attr;
}

static void
cgactl(int cmd, int a, int b)
{
  int col = cga_pos % TTY_COLS, row = cga_pos / TTY_COLS;
  int first, last;

  switch(cmd){
  case SCREEN_SET:
    col = a < 0 ? col : screenbound(a, TTY_COLS);
    row = b < 0 ? row : screenbound(b, TTY_ROWS);
    cga_pos = row * TTY_COLS + col;
    break;
  case SCREEN_MOVE:
    cga_pos = screenbound(row+b, TTY_ROWS) * TTY_COLS +
              screenbound(col+a, TTY_COLS);
    break;
  case SCREEN_SAVE:
    cga_saved = cga_pos;
    break;
  case SCREEN_RESTORE:
    cga_pos = cga_saved;
    break;
  case SCREEN_CURSOR:
    outb(CRTPORT, 10);
    outb(CRTPORT+1, a ? 0x0e : 0x20);
    break;
  case SCREEN_DISPLAY: case SCREEN_LINE:
    if(a < 0 || a > 2)
      break;
    first = cmd == SCREEN_LINE ? row * TTY_COLS : 0;
    last = cmd == SCREEN_LINE ? first + TTY_COLS : TTY_ROWS * TTY_COLS;
    cgaerase(a == 0 ? cga_pos : first, a == 1 ? cga_pos+1 : last);
    break;
  }
  cgacursor();
}

static void
cgaputc(int c)
{
  if(c == 0x0c){
    cgaerase(0, TTY_COLS * TTY_ROWS);
    cga_pos = 0;
  } else if(c == '\n'){
    cga_pos += TTY_COLS - cga_pos % TTY_COLS;
  } else if(c == '\r'){
    cga_pos -= cga_pos % TTY_COLS;
  } else if(c == BACKSPACE){
    if(cga_pos > 0)
      crt[--cga_pos] = ' ' | cga_attr;
  } else if(c == '\t'){
    do {
      cgaputc(' ');
    } while(cga_pos % 8);
    return;
  } else {
    crt[cga_pos++] = (c & 0xff) | cga_attr;
  }
  if(cga_pos >= TTY_COLS * TTY_ROWS){
    memmove(crt, crt+TTY_COLS, sizeof(*crt)*TTY_COLS*(TTY_ROWS-1));
    cga_pos -= TTY_COLS;
    cgaerase(cga_pos, TTY_COLS * TTY_ROWS);
  }
  cgacursor();
}

static void
displayctl(int cmd, int a, int b)
{
  if(fbactive())
    fbctl(cmd, a, b);
  else
    cgactl(cmd, a, b);
}

static void
style(int code)
{
  static const uchar vga[] = {0,4,2,6,1,5,3,7};
  static int fg = 7, bg, bold, reverse;
  int fore, back;

  if(code == 0){
    fg = 7;
    bg = bold = reverse = 0;
  } else if(code == 1 || code == 22){
    bold = code == 1;
  } else if(code == 7 || code == 27){
    reverse = code == 7;
  } else if(code == 39){
    fg = 7;
  } else if(code == 49){
    bg = 0;
  } else if(code >= 30 && code <= 37){
    fg = code - 30;
  } else if(code >= 40 && code <= 47){
    bg = code - 40;
  } else if(code >= 90 && code <= 97){
    fg = code - 90 + 8;
  } else if(code >= 100 && code <= 107){
    bg = code - 100 + 8;
  }
  fore = fg | (bold ? 8 : 0);
  back = bg;
  if(reverse){
    back = fore;
    fore = bg;
  }
  if(fbactive())
    fbstyle(fore, back);
  else
    cga_attr = ((vga[fore&7] | (fore&8)) << 8) | (vga[back&7] << 12);
}

static void
sequence(int c, int *params, int count, int private)
{
  int a = params[0], n = a ? a : 1;
  int b = count > 1 && params[1] ? params[1] : 1;
  int i;

  if(private){
    if(count == 1 && a == 25 && (c == 'h' || c == 'l'))
      displayctl(SCREEN_CURSOR, c == 'h', 0);
    return;
  }
  switch(c){
  case 'A': displayctl(SCREEN_MOVE, 0, -n); break;
  case 'B': displayctl(SCREEN_MOVE, 0, n); break;
  case 'C': displayctl(SCREEN_MOVE, n, 0); break;
  case 'D': displayctl(SCREEN_MOVE, -n, 0); break;
  case 'E': case 'F':
    displayctl(SCREEN_MOVE, 0, c == 'E' ? n : -n);
    displayctl(SCREEN_SET, 0, -1);
    break;
  case 'G': displayctl(SCREEN_SET, n-1, -1); break;
  case 'd': displayctl(SCREEN_SET, -1, n-1); break;
  case 'H': case 'f': displayctl(SCREEN_SET, b-1, n-1); break;
  case 'J': displayctl(SCREEN_DISPLAY, a, 0); break;
  case 'K': displayctl(SCREEN_LINE, a, 0); break;
  case 's': displayctl(SCREEN_SAVE, 0, 0); break;
  case 'u': displayctl(SCREEN_RESTORE, 0, 0); break;
  case 'm':
    for(i = 0; i < count; i++){
      // skip extended color arguments rather than treating them as standalone styles
      if(params[i] == 38 || params[i] == 48){
        if(i+1 < count && params[i+1] == 5)
          i += 2;
        else if(i+1 < count && params[i+1] == 2)
          i += 4;
        else
          i++;
      } else {
        style(params[i]);
      }
    }
    break;
  }
}

// keep escape parsing local while the serial terminal gets the original bytes
static void
displayputc(int c)
{
  static int state, params[16], count, skip, private;

  // swallow terminal strings until bell or the string terminator
  if(state >= 3 && c != 0x0c){
    if(c == 7 || c == 0x18 || c == 0x1a || (state == 4 && c == 0x5c))
      state = 0;
    else
      state = c == 0x1b ? 4 : 3;
    return;
  }
  if(c == 0x1b){
    state = 1;
    return;
  }
  if(c == 0x0c)
    state = 0;
  if(state == 1){
    state = 0;
    if(c == '['){
      state = 2;
      count = 1;
      params[0] = 0;
      skip = private = 0;
      return;
    }
    if(c == ']' || c == 'P' || c == '_' || c == '^'){
      state = 3;
      return;
    }
    if(c == '7' || c == '8'){
      displayctl(c == '7' ? SCREEN_SAVE : SCREEN_RESTORE, 0, 0);
      return;
    }
    if(c == 'c'){
      style(0);
      displayctl(SCREEN_CURSOR, 1, 0);
      c = 0x0c;
    } else {
      return;
    }
  } else if(state == 2){
    if(c >= '0' && c <= '9'){
      if(params[count-1] < 1000)
        params[count-1] = params[count-1] * 10 + c - '0';
      return;
    }
    if(c == ';'){
      if(count < NELEM(params))
        params[count++] = 0;
      else
        skip = 1;
      return;
    }
    if(c == '?' && count == 1 && params[0] == 0 && !private){
      private = 1;
      return;
    }
    if(c == 0x18 || c == 0x1a){
      state = 0;
      return;
    }
    if(c < '@' || c > '~'){
      skip = 1;
      return;
    }
    state = 0;
    if(!skip)
      sequence(c, params, count, private);
    return;
  }

  if(fbactive())
    fbputc(c);
  else
    cgaputc(c);
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else if(c == 0x0c){
    const char *sequence = "\033[2J\033[H";
    while(*sequence)
      uartputc(*sequence++);
  } else {
    uartputc(c);
  }
  displayputc(c);
}

#define INPUT_BUF 512
struct {
  ushort buf[INPUT_BUF];
  uint r;  // next byte for read
  uint w;  // end of input readers can see
  uint e;  // end of the line being edited
} input;

// keep pager keys apart from incomplete or queued shell input
static struct {
  uchar buf[INPUT_BUF];
  uint r, w;
  int pid;
} cbreak;

#define C(x)  ((x)-'@')  // Control-x

// keep literal control bytes distinct from the eof marker
#define QUOTED 0x100
#define TTY_EOF 0x200

static void
echoinput(int c)
{
  if(c == '\n' || c >= ' '){
    consputc(c);
  } else {
    consputc('^');
    consputc(c + '@');
  }
}

static void
eraselast(int echo)
{
  int ch = input.buf[--input.e % INPUT_BUF] & 0xff;

  if(echo){
    consputc(BACKSPACE);
    if(ch < ' ')
      consputc(BACKSPACE);
  }
}

static void
reprint(void)
{
  uint i;

  for(i = input.w; i != input.e; i++)
    echoinput(input.buf[i % INPUT_BUF] & 0xff);
}

static const char *
keysequence(int key)
{
  switch(key){
  case KEY_UP:   return "\033[A";
  case KEY_DN:   return "\033[B";
  case KEY_RT:   return "\033[C";
  case KEY_LF:   return "\033[D";
  case KEY_HOME: return "\033[H";
  case KEY_END:  return "\033[F";
  case KEY_PGUP: return "\033[5~";
  case KEY_PGDN: return "\033[6~";
  case KEY_INS:  return "\033[2~";
  case KEY_DEL:  return "\033[3~";
  }
  return "";
}

static void
queuekey(int pid, int c, int echo)
{
  char byte = c;
  const char *keys = &byte;
  uint n = 1;

  if(c & KBD_SPECIAL){
    keys = keysequence(c & 0xff);
    n = strlen(keys);
    echo = 0;
  }
  if(pid){
    if(cbreak.pid != pid){
      cbreak.pid = pid;
      cbreak.r = cbreak.w = 0;
    }
    if(n > INPUT_BUF - (cbreak.w - cbreak.r))
      return;
    while(n--)
      cbreak.buf[cbreak.w++ % INPUT_BUF] = *keys++;
    wakeup(&cbreak.r);
    selectnotify();
  } else {
    if(n > INPUT_BUF - (input.e - input.r))
      return;
    while(n--)
      input.buf[input.e++ % INPUT_BUF] = (uchar)*keys++;
    input.w = input.e;
    if(echo)
      echoinput(c);
    wakeup(&input.r);
    selectnotify();
  }
}

static int
matches(const struct termios *attr, int index, int c)
{
  return attr->c_cc[index] != _POSIX_VDISABLE && attr->c_cc[index] == c;
}

static void
resumeoutput(void)
{
  if(paused){
    paused = 0;
    wakeup(&paused);
    selectnotify();
  }
}

static int
signals(const struct termios *attr, int c, int owner, int echo)
{
  int sig;

  if(!(attr->c_lflag & ISIG))
    return 0;
  if(matches(attr, VINTR, c))
    sig = SIGINT;
  else if(matches(attr, VQUIT, c))
    sig = SIGQUIT;
  else if(matches(attr, VSUSP, c))
    sig = SIGTSTP;
  else
    return 0;

  // discard typeahead so it cannot reach the shell after an interrupt
  input.e = input.w = input.r;
  if(owner == cbreak.pid)
    cbreak.r = cbreak.w;
  resumeoutput();
  if(echo){
    if(owner){
      const char *erase = "\r\033[K";
      while(*erase)
        consputc(*erase++);
    }
    echoinput(c);
    consputc('\n');
  }
  ttysignal(sig);
  return 1;
}

static int
flow(const struct termios *attr, int c)
{
  if(!(attr->c_iflag & IXON))
    return 0;
  if(matches(attr, VSTOP, c))
    paused = 1;
  else if(matches(attr, VSTART, c))
    resumeoutput();
  else
    return 0;
  return 1;
}

// return true when an editing key has already been handled
static int
editline(const struct termios *attr, int c, int echo)
{
  if(matches(attr, VERASE, c) || (attr->c_cc[VERASE] == C('H') && c == 0x7f)){
    if(input.e != input.w)
      eraselast(echo);
  } else if(matches(attr, VKILL, c)){
    while(input.e != input.w)
      eraselast(echo);
  } else if(!(attr->c_lflag & IEXTEN)){
    return 0;
  } else if(matches(attr, VWERASE, c)){
    while(input.e != input.w && (input.buf[(input.e-1) % INPUT_BUF] & 0xff) == ' ')
      eraselast(echo);
    while(input.e != input.w && (input.buf[(input.e-1) % INPUT_BUF] & 0xff) != ' ')
      eraselast(echo);
  } else if(matches(attr, VREPRINT, c) || c == C('L')){
    if(echo){
      consputc(c == C('L') ? C('L') : '\n');
      reprint();
    }
  } else if(matches(attr, VLNEXT, c)){
    literal = 1;
  } else {
    return 0;
  }
  return 1;
}

static void
consoleinput(int (*getc)(void), int keyboard)
{
  const struct termios *attr;
  int c, owner, canonical, echo, quoted, eof, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    if(c == KBD_NOCHAR || (keyboard && cons.framebuffer_users))
      continue;
    owner = ttyinputowner(0);
    attr = echo_owner ? &defaults : &settings;
    canonical = !owner && (attr->c_lflag & ICANON);
    echo = !echo_owner && (attr->c_lflag & ECHO);
    if(canonical && (c & KBD_SPECIAL))
      continue;
    quoted = canonical && literal;
    literal = 0;

    if(!quoted){
      if(c == '\r' && (attr->c_iflag & ICRNL))
        c = '\n';
      if(signals(attr, c, owner, echo) || flow(attr, c))
        continue;
    }
    if(!canonical){
      queuekey(owner, c, echo && !owner);
      continue;
    }
    if(!quoted && editline(attr, c, echo))
      continue;
    if(!quoted && (attr->c_lflag & IEXTEN) && c == C('P')){
      if(echo)
        doprocdump = 1;
      continue;
    }
    if(input.e-input.r >= INPUT_BUF)
      continue;
    eof = !quoted && matches(attr, VEOF, c);
    input.buf[input.e++ % INPUT_BUF] = eof ? TTY_EOF : quoted ? QUOTED | c : c;
    if(echo && !eof){
      if(quoted && c == '\n'){
        consputc('^');
        consputc('J');
      } else {
        echoinput(c);
      }
    }
    if(eof || (!quoted && c == '\n') || input.e-input.r == INPUT_BUF){
      input.w = input.e;
      wakeup(&input.r);
      selectnotify();
    }
  }
  fbflush();
  release(&cons.lock);
  if(doprocdump)
    procdump();
}

void
consoleintr(int (*getc)(void))
{
  consoleinput(getc, 0);
}

void
consolekeyintr(int (*getc)(void))
{
  consoleinput(getc, 1);
}

// a cbreak read returns the keys already available without waiting for a line
static int
cbreakread(int pid, char *dst, int n, int nonblock)
{
  int count = 0;

  if(!n)
    return 0;
  while(cbreak.pid != pid || cbreak.r == cbreak.w){
    if(nonblock)
      return -P9_EAGAIN;
    if(ttyinputowner(0) != pid || sleepintr(&cbreak.r, &cons.lock) < 0)
      return -1;
  }
  if(ttyinputowner(0) != pid)
    return -1;
  while(count < n && cbreak.r != cbreak.w)
    dst[count++] = cbreak.buf[cbreak.r++ % INPUT_BUF];
  return count;
}

int
consoleread(struct inode *ip, char *dst, int n, uint off, int nonblock)
{
  int target = n, c, canonical;

  iunlock(ip);
  acquire(&cons.lock);
  if(myproc()->tty_cbreak){
    int result = cbreakread(myproc()->pid, dst, n, nonblock);
    release(&cons.lock);
    ilock(ip);
    return result;
  }
  while(n > 0){
    if(!ttyforeground()){
      if(n == target)
        n = target + 1;
      break;
    }
    canonical = echo_owner || (settings.c_lflag & ICANON);
    if(input.r == input.w){
      if(nonblock){
        if(n == target)
          n = target + P9_EAGAIN;
        break;
      }
      if(!canonical && n != target)
        break;
      if(sleepintr(&input.r, &cons.lock) < 0){
        if(n == target)
          n = target + 1;
        break;
      }
      continue;
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == TTY_EOF){
      if(!canonical)
        continue;
      // return any bytes already read and leave eof for the next read
      if(n < target)
        input.r--;
      break;
    }
    *dst++ = c & 0xff;
    --n;
    if(canonical && c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);
  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n, int nonblock)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++){
    while(paused){
      if(nonblock){
        release(&cons.lock);
        ilock(ip);
        return i ? i : -P9_EAGAIN;
      }
      fbflush();
      if(sleepintr(&paused, &cons.lock) < 0){
        release(&cons.lock);
        ilock(ip);
        return i ? i : -1;
      }
    }
    consputc(buf[i] & 0xff);
    if((i & 63) == 63 && i + 1 < n){
      fbflush();
      release(&cons.lock);
      acquire(&cons.lock);
    }
  }
  fbflush();
  release(&cons.lock);
  ilock(ip);

  return n;
}

static int
consoleready(struct inode *ip, int writing)
{
  struct proc *p = myproc();
  int ready;

  acquire(&cons.lock);
  if(writing)
    ready = !paused;
  else if(p->tty_cbreak)
    ready = cbreak.pid == p->pid && ttyinputowner(0) == p->pid && cbreak.r != cbreak.w;
  else
    ready = ttyforeground() && input.r != input.w;
  release(&cons.lock);
  return ready;
}

void
consoleinit(void)
{
  int cols, rows;

  fbtextsize(&cols, &rows);
  if(viewport.ws_col > cols)
    viewport.ws_col = cols;
  if(viewport.ws_row > rows)
    viewport.ws_row = rows;
  initlock(&cons.lock, "console");
  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].ready = consoleready;
  cons.locking = 1;
  ioapicenable(IRQ_KBD, 0);
}

int
consoleioctl(int request, void *data)
{
  struct termios wanted;
  struct winsize size;
  int cols, rows, result = -1;

  acquire(&cons.lock);
  if(request == TIOCGETA){
    memmove(data, &settings, sizeof(settings));
    result = 0;
  } else if(request == TIOCGWINSZ){
    memmove(data, &viewport, sizeof(viewport));
    result = 0;
  } else if(ttyforeground() && !echo_owner){
    if(request == TIOCSWINSZ){
      memmove(&size, data, sizeof(size));
      fbtextsize(&cols, &rows);
      if(size.ws_row >= 2 && size.ws_col >= 2 &&
         size.ws_row <= rows && size.ws_col <= cols){
        size.ws_xpixel = size.ws_ypixel = 0;
        viewport = size;
        result = 0;
      }
    } else {
      memmove(&wanted, data, sizeof(wanted));
      if(!(wanted.c_iflag & ~(ICRNL | IXON)) &&
         !(wanted.c_lflag & ~(ECHO | ICANON | ISIG | IEXTEN)) &&
         !wanted.c_oflag && !wanted.c_cflag &&
         wanted.c_cc[VMIN] == 1 && wanted.c_cc[VTIME] == 0){
        // output is synchronous so there is no output queue to drain
        if(request == TIOCSETAF){
          input.e = input.w = input.r;
          cbreak.r = cbreak.w;
          literal = 0;
        } else if((wanted.c_lflag ^ settings.c_lflag) & ICANON) {
          input.w = input.e;
          literal = 0;
          wakeup(&input.r);
          selectnotify();
        }
        settings = wanted;
        if(!(settings.c_iflag & IXON))
          resumeoutput();
        result = 0;
      }
    }
  }
  release(&cons.lock);
  return result;
}

int
setecho(int enable)
{
  int pid = myproc()->pid;
  int result = -1;

  acquire(&cons.lock);
  if(ttyforeground() && !ttyinputowner(0) && (!echo_owner || echo_owner == pid)){
    if(!enable && !echo_owner){
      input.e = input.w = input.r;
      literal = 0;
      resumeoutput();
    }
    echo_owner = enable ? 0 : pid;
    result = 0;
  }
  release(&cons.lock);
  return result;
}

int
consolemode(int request)
{
  int pid = myproc()->pid;
  int owner, result = -1;

  acquire(&cons.lock);
  if(request == TTY_CBREAK){
    owner = ttyinputowner(0);
    if(!echo_owner && (!owner || owner == pid) && ttysetcbreak(0, 1) == 0){
      if(cbreak.pid != pid){
        cbreak.pid = pid;
        cbreak.r = cbreak.w = 0;
      }
      result = 0;
    }
  } else if(request == TTY_CANON){
    result = ttysetcbreak(0, 0);
    if(cbreak.pid == pid)
      cbreak.pid = 0;
  }
  release(&cons.lock);
  return result;
}

void
ttyrestore(void)
{
  int pid = myproc()->pid;

  acquire(&cons.lock);
  if(myproc()->tty_cbreak){
    struct file *tty = myproc()->ctty;
    ttysetcbreak(tty ? tty->pty : 0, 0);
    if(cbreak.pid == pid)
      cbreak.pid = 0;
  }
  if(echo_owner == pid){
    echo_owner = 0;
    input.e = input.w = input.r;
    literal = 0;
    resumeoutput();
  }
  release(&cons.lock);
}

static int
framebuffer_available(int minor)
{
  return minor == 0 && fbactive();
}

static int
framebuffer_read(struct inode *ip, char *dst, int n, uint off, int nonblock)
{
  int result;

  if(!framebuffer_available(ip->minor))
    return -1;
  acquire(&cons.lock);
  result = fbpixels(dst, n, off);
  release(&cons.lock);
  return result;
}

void
fbdevinit(void)
{
  devsw[DEV_FB].read = framebuffer_read;
  devsw[DEV_FB].available = framebuffer_available;
}

void
fbgraphics(int delta)
{
  acquire(&cons.lock);
  cons.framebuffer_users += delta;
  fbdisplay(cons.framebuffer_users);
  release(&cons.lock);
}
