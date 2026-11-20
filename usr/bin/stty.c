/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "ioctl.h"
#include "tty.h"
#include "user.h"

static const struct {
  const char *name;
  tcflag_t bit;
  int input;
} flags[] = {
  {"icrnl", ICRNL, 1}, {"ixon", IXON, 1},
  {"echo", ECHO, 0}, {"icanon", ICANON, 0},
  {"isig", ISIG, 0}, {"iexten", IEXTEN, 0},
};

static const struct {
  const char *name;
  int index;
} controls[] = {
  {"intr", VINTR}, {"quit", VQUIT}, {"susp", VSUSP}, {"eof", VEOF},
  {"erase", VERASE}, {"kill", VKILL}, {"werase", VWERASE},
  {"rprnt", VREPRINT}, {"lnext", VLNEXT}, {"stop", VSTOP}, {"start", VSTART},
};

enum {
  FLAG_COUNT = sizeof(flags) / sizeof(flags[0]),
  CONTROL_COUNT = sizeof(controls) / sizeof(controls[0])
};

static void
printcontrol(int value)
{
  if(value == _POSIX_VDISABLE)
    printf(1, "undef");
  else if(value < ' ')
    printf(1, "^%c", value + '@');
  else if(value == 127)
    printf(1, "^?");
  else if(value > 127)
    printf(1, "%d", value);
  else
    printf(1, "%c", value);
}

static void
show(const struct termios *attr, const struct winsize *size)
{
  uint i;
  tcflag_t bits;

  printf(1, "rows %d; columns %d; min %d; time %d\n",
         size->ws_row, size->ws_col, attr->c_cc[VMIN], attr->c_cc[VTIME]);
  for(i = 0; i < FLAG_COUNT; i++){
    bits = flags[i].input ? attr->c_iflag : attr->c_lflag;
    printf(1, "%s%s%s", bits & flags[i].bit ? "" : "-", flags[i].name,
           i + 1 == FLAG_COUNT ? "\n" : " ");
  }
  for(i = 0; i < CONTROL_COUNT; i++){
    printf(1, "%s = ", controls[i].name);
    printcontrol(attr->c_cc[controls[i].index]);
    printf(1, i + 1 == CONTROL_COUNT ? "\n" : "; ");
  }
}

static int
controlvalue(const char *s)
{
  uint value;

  if(!strcmp(s, "undef"))
    return _POSIX_VDISABLE;
  if(parseuint(s, &value) == 0)
    return value <= 255 ? (int)value : -1;
  if(s[0] && !s[1])
    return (uchar)s[0];
  if(s[0] == '^' && s[1] && !s[2]){
    if(s[1] == '?')
      return 127;
    if(s[1] >= 'a' && s[1] <= 'z')
      return s[1] - 'a' + 1;
    if(s[1] >= '@' && s[1] <= '_')
      return s[1] - '@';
  }
  return -1;
}

static int
setflag(struct termios *attr, const char *name)
{
  uint i;
  int enable = *name != '-';
  tcflag_t *bits;

  if(!enable)
    name++;
  for(i = 0; i < FLAG_COUNT; i++){
    if(strcmp(name, flags[i].name))
      continue;
    bits = flags[i].input ? &attr->c_iflag : &attr->c_lflag;
    if(enable)
      *bits |= flags[i].bit;
    else
      *bits &= ~flags[i].bit;
    return 1;
  }
  return 0;
}

int
main(int argc, char **argv)
{
  const struct termios defaults = TTY_DEFAULTS;
  struct termios attr;
  struct winsize size;
  int arg, rows, val, change = 0, resize = 0;
  uint i, n;
  const char *name;

  if(tcgetattr(0, &attr) < 0 || ioctl(0, TIOCGWINSZ, &size) < 0){
    printf(2, "stty: standard input is not a terminal\n");
    return 1;
  }
  if(argc == 1 || (argc == 2 && !strcmp(argv[1], "-a"))){
    show(&attr, &size);
    return 0;
  }
  if(argc == 2 && !strcmp(argv[1], "size")){
    printf(1, "%d %d\n", size.ws_row, size.ws_col);
    return 0;
  }
  for(arg = 1; arg < argc; arg++){
    name = argv[arg];
    if(!strcmp(name, "sane"))
      attr = defaults;
    else if(!strcmp(name, "raw"))
      cfmakeraw(&attr);
    else if(setflag(&attr, name)){
      change = 1;
      continue;
    } else if((rows = !strcmp(name, "rows")) || !strcmp(name, "columns") ||
              !strcmp(name, "cols")){
      if(++arg == argc || parseuint(argv[arg], &n) < 0 || n > 65535)
        goto usage;
      if(rows)
        size.ws_row = n;
      else
        size.ws_col = n;
      resize = 1;
      continue;
    } else {
      for(i = 0; i < CONTROL_COUNT && strcmp(name, controls[i].name); i++)
        ;
      if(i == CONTROL_COUNT || ++arg == argc || (val = controlvalue(argv[arg])) < 0)
        goto usage;
      attr.c_cc[controls[i].index] = val;
    }
    change = 1;
  }
  if(resize && ioctl(0, TIOCSWINSZ, &size) < 0){
    printf(2, "stty: cannot set terminal size\n");
    return 1;
  }
  if(change && tcsetattr(0, TCSANOW, &attr) < 0){
    printf(2, "stty: cannot set terminal attributes\n");
    return 1;
  }
  return 0;

usage:
  printf(2, "usage: stty [-a | size | setting ...]\n");
  return 2;
}
