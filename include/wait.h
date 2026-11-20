/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_WAIT_H
#define PROJECT9_WAIT_H

#define WNOHANG    1
#define WUNTRACED  2
#define WCONTINUED 8

#define WIFEXITED(s)    (((s) & 0x7f) == 0)
#define WEXITSTATUS(s)  (((s) >> 8) & 0xff)
#define WIFCONTINUED(s) ((s) == 0xffff)
#define WIFSTOPPED(s)   (((s) & 0xff) == 0x7f)
#define WSTOPSIG(s)     WEXITSTATUS(s)
#define WIFSIGNALED(s)  (((s) & 0x7f) != 0 && ((s) & 0x7f) != 0x7f)
#define WTERMSIG(s)     ((s) & 0x7f)
#define WCOREDUMP(s)    ((s) & 0x80)

#endif
