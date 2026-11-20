/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "signal.h"

static int
signumber(const char *name)
{
  static const char *names[NSIG] = {
    [SIGINT] = "INT", [SIGQUIT] = "QUIT", [SIGKILL] = "KILL",
    [SIGSEGV] = "SEGV", [SIGTERM] = "TERM", [SIGCHLD] = "CHLD",
    [SIGCONT] = "CONT", [SIGSTOP] = "STOP", [SIGTSTP] = "TSTP"
  };
  uint n;

  if(parseuint(name, &n) == 0)
    return n == 0 || (n < NSIG && names[n]) ? (int)n : -1;
  if(name[0] == 'S' && name[1] == 'I' && name[2] == 'G')
    name += 3;
  for(n = 1; n < NSIG; n++)
    if(names[n] && !strcmp(name, names[n]))
      return n;
  return -1;
}

int
main(int argc, char **argv)
{
  int i = 1, sig = SIGTERM, failed = 0, negative;
  const char *target;
  uint number;

  if(i < argc && !strcmp(argv[i], "-s")){
    if(++i >= argc || (sig = signumber(argv[i++])) < 0)
      goto usage;
  }
  if(i < argc && !strcmp(argv[i], "--"))
    i++;
  if(i >= argc)
    goto usage;
  for(; i < argc; i++){
    target = argv[i];
    negative = *target == '-';
    if(parseuint(target + negative, &number) < 0 ||
       send_signal(negative ? -(int)number : (int)number, sig) < 0){
      printf(2, "kill: cannot signal %s\n", target);
      failed = 1;
    }
  }
  exitstatus(failed);

usage:
  printf(2, "usage: kill [-s signal] [--] pid ...\n");
  exitstatus(1);
}
