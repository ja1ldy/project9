/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_TTY_H
#define PROJECT9_TTY_H
#include "signal.h"
#include "termios.h"

#define JOB_STOPPED 1
#define JOB_RUNNING 2
#define JOB_FOREGROUND 0
#define JOB_BACKGROUND 1
#define JOB_RESUME 1
#define JOB_SESSION 2
#define JOB_CONT_BACKGROUND 3
#define JOB_POLL (-1)

#define TTY_ISATTY 0
#define TTY_CBREAK 1
#define TTY_CANON 2

// the default viewport fits the serial terminal and vga text console
#define TTY_COLS 80
#define TTY_ROWS 24
#define TTY_MAX_COLS 85
#define TTY_MAX_ROWS 32

#define TTY_DEFAULTS { \
  .c_iflag = ICRNL | IXON, \
  .c_lflag = ECHO | ICANON | ISIG | IEXTEN, \
  .c_cc = { \
    [VINTR] = 3, [VQUIT] = 28, [VSUSP] = 26, [VEOF] = 4, \
    [VERASE] = 8, [VKILL] = 21, [VWERASE] = 23, [VREPRINT] = 18, \
    [VLNEXT] = 22, [VSTOP] = 19, [VSTART] = 17, [VMIN] = 1, [VTIME] = 0 \
  } \
}
#endif
