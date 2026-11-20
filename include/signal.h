/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_SIGNAL_H
#define PROJECT9_SIGNAL_H

#define SIGHUP  1
#define SIGINT  2
#define SIGQUIT 3
#define SIGKILL 9
#define SIGSEGV 11
#define SIGPIPE 13
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGWINCH 28
#define NSIG    29

#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

#endif
