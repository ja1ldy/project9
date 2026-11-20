/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SIGNAL_H
#define P9_LIBC_SIGNAL_H
#include <sys/types.h>
#include <project9/signal.h>
typedef unsigned int sigset_t;
typedef int sig_atomic_t;
#define SIG_ERR ((void (*)(int))-1)
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
struct sigaction {
  void (*sa_handler)(int);
  sigset_t sa_mask;
  int sa_flags;
};
int sigaction(int, const struct sigaction *, struct sigaction *) __asm__("p9_sigaction");
int sigprocmask(int, const sigset_t *, sigset_t *);
int sigemptyset(sigset_t *), sigfillset(sigset_t *), sigaddset(sigset_t *, int);
int sigdelset(sigset_t *, int), sigismember(const sigset_t *, int);
void (*signal(int, void (*)(int)))(int) __asm__("p9_signal");
int kill(pid_t, int) __asm__("p9_kill");
int raise(int);
#endif
