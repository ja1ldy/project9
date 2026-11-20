/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_TERMIOS_H
#define PROJECT9_TERMIOS_H

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;

#define NCCS 13

struct termios {
  tcflag_t c_iflag, c_oflag, c_cflag, c_lflag;
  cc_t c_cc[NCCS];
};

#define ICRNL  (1U << 0)
#define IXON   (1U << 1)

#define OPOST  (1U << 0)
#define ONLCR  (1U << 1)

#define ECHO   (1U << 0)
#define ICANON (1U << 1)
#define ISIG   (1U << 2)
#define IEXTEN (1U << 3)

#define VINTR     0
#define VQUIT     1
#define VSUSP     2
#define VEOF      3
#define VERASE    4
#define VKILL     5
#define VWERASE   6
#define VREPRINT  7
#define VLNEXT    8
#define VSTOP     9
#define VSTART   10
#define VMIN     11
#define VTIME    12

#define _POSIX_VDISABLE 0
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

int tcgetattr(int, struct termios *);
int tcsetattr(int, int, const struct termios *);
void cfmakeraw(struct termios *);
int isatty(int);

#endif
