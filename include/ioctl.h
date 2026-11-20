/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_IOCTL_H
#define PROJECT9_IOCTL_H

#define TIOCGETA 0x7401
#define TIOCSETA 0x7402
#define TIOCSETAW 0x7403
#define TIOCSETAF 0x7404
#define TIOCGWINSZ 0x7405
#define TIOCSWINSZ 0x7406
#define TIOCSCTTY 0x7407
#define TIOCGPTY  0x7408
#define FIONREAD  0x6604

struct winsize {
  unsigned short ws_row, ws_col;
  unsigned short ws_xpixel, ws_ypixel;
};

int ioctl(int, int, void *);

#endif
