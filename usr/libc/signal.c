/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>

extern int p9_result(int, int);
extern int send_signal(int, int), sigmask(int, unsigned);
extern int sigactionx(int, unsigned, void (*)(void), unsigned, unsigned *);

#ifdef P9_USER64
extern void p9_sigtramp(void);
#else
extern int sigreturn(void);

static void
p9_sigtramp(void)
{
  sigreturn();
  _exit(1);
}
#endif

int
sigaction(int sig, const struct sigaction *action, struct sigaction *old)
{
  unsigned mask = 0;
  int handler;

  if(action && (action->sa_flags || action->sa_handler == SIG_ERR))
    return p9_result(-1, EINVAL);
  handler = p9_result(sigactionx(sig, action ? (uintptr_t)action->sa_handler : ~0U,
                                p9_sigtramp, action ? action->sa_mask : 0, &mask), EINVAL);
  if(handler < 0)
    return -1;
  if(old)
    *old = (struct sigaction){ (void (*)(int))(uintptr_t)handler, mask, 0 };
  return 0;
}

void (*
signal(int sig, void (*handler)(int)))(int)
{
  struct sigaction action = { .sa_handler = handler }, old = {0};
  return sigaction(sig, &action, &old) < 0 ? SIG_ERR : old.sa_handler;
}

int
sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
  int mask;

  if(set && (how < SIG_BLOCK || how > SIG_SETMASK))
    return p9_result(-1, EINVAL);
  mask = p9_result(sigmask(set ? how : -1, set ? *set : 0), EINVAL);
  if(mask < 0)
    return -1;
  if(old)
    *old = mask;
  return 0;
}

int sigemptyset(sigset_t *set) { *set = 0; return 0; }
int sigfillset(sigset_t *set) { *set = (1U << NSIG) - 2; return 0; }

static int valid(int sig) { return sig > 0 && sig < NSIG; }

int
sigaddset(sigset_t *set, int sig)
{
  if(!valid(sig))
    return p9_result(-1, EINVAL);
  *set |= 1U << sig;
  return 0;
}

int
sigdelset(sigset_t *set, int sig)
{
  if(!valid(sig))
    return p9_result(-1, EINVAL);
  *set &= ~(1U << sig);
  return 0;
}

int sigismember(const sigset_t *set, int sig) { return valid(sig) ? !!(*set & (1U<<sig)) : p9_result(-1, EINVAL); }
int kill(pid_t pid, int sig) { return p9_result(send_signal(pid, sig), EINVAL); }
int raise(int sig) { return kill(getpid(), sig); }
