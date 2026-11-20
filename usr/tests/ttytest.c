/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "ioctl.h"
#include "termios.h"
#include "user.h"

static int
attributes(const struct termios *saved)
{
  struct termios next = *saved, check;
  int pipefd[2], failed;

  if(pipe(pipefd) < 0)
    return -1;
  failed = isatty(pipefd[0]) || ioctl(pipefd[0], TIOCGETA, &check) == 0;
  close(pipefd[0]);
  close(pipefd[1]);

  cfmakeraw(&next);
  next.c_cc[VERASE] = 127;
  if(failed || tcsetattr(0, TCSANOW, &next) < 0)
    return -1;
  if(tcgetattr(0, &check) < 0 || check.c_iflag || check.c_lflag ||
     check.c_cc[VERASE] != 127)
    return -1;
  next.c_cc[VMIN] = 2;
  if(tcsetattr(0, TCSANOW, &next) == 0 || tcgetattr(0, &check) < 0 ||
     check.c_cc[VMIN] != 1)
    return -1;
  return 0;
}

static int
dimensions(void)
{
  struct winsize saved, check, next;
  int failed;

  if(ioctl(0, TIOCGWINSZ, &saved) < 0)
    return -1;
  next = saved;
  next.ws_row = 19;
  next.ws_col = 72;
  if(ioctl(0, TIOCSWINSZ, &next) < 0)
    return -1;
  failed = ioctl(0, TIOCGWINSZ, &check) < 0 ||
           check.ws_row != next.ws_row || check.ws_col != next.ws_col;
  next.ws_col = 0;
  if(ioctl(0, TIOCSWINSZ, &next) == 0)
    failed = 1;
  if(ioctl(0, TIOCSWINSZ, &saved) < 0)
    failed = 1;
  return failed ? -1 : 0;
}

int
main(void)
{
  struct termios saved;
  int failed;

  if(tcgetattr(0, &saved) < 0){
    printf(2, "ttytest: run from the console\n");
    return 1;
  }
  failed = attributes(&saved) < 0;
  if(tcsetattr(0, TCSAFLUSH, &saved) < 0 || dimensions() < 0)
    failed = 1;
  if(failed){
    printf(2, "ttytest: terminal check failed\n");
    return 1;
  }
  printf(1, "TTY TESTS PASSED\n");
  return 0;
}
