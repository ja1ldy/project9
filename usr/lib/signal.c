/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

#ifdef P9_USER64
void p9_sigtramp(void);
#else
// returning from a handler takes this route back to the interrupted code
static void
p9_sigtramp(void)
{
  sigreturn();
  exit();
}
#endif

void (*
signal(int sig, void (*handler)(int)))(int)
{
  return (void (*)(int))(long)sigaction(sig, handler, p9_sigtramp);
}
