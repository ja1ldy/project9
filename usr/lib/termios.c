/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "ioctl.h"
#include "termios.h"

int
tcgetattr(int fd, struct termios *attributes)
{
  return ioctl(fd, TIOCGETA, attributes);
}

int
tcsetattr(int fd, int action, const struct termios *attributes)
{
  int request;

  switch(action){
  case TCSANOW:   request = TIOCSETA; break;
  case TCSADRAIN: request = TIOCSETAW; break;
  case TCSAFLUSH: request = TIOCSETAF; break;
  default: return -1;
  }
  return ioctl(fd, request, (void*)attributes);
}

void
cfmakeraw(struct termios *attributes)
{
  attributes->c_iflag &= ~(ICRNL | IXON);
  attributes->c_oflag &= ~OPOST;
  attributes->c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  attributes->c_cc[VMIN] = 1;
  attributes->c_cc[VTIME] = 0;
}

int
isatty(int fd)
{
  struct termios attributes;

  return tcgetattr(fd, &attributes) == 0;
}
