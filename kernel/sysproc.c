/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "procinfo.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "tty.h"
#include "mode.h"
#include "wait.h"
#include "usbhost.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0 || n < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    if(sleepintr(&ticks, &tickslock) < 0){
      release(&tickslock);
      return -1;
    }
  }
  release(&tickslock);
  return 0;
}

// return the number of timer ticks since boot
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_cps(void)
{
  return cps();
}

int
sys_freemem(void)
{
  return freemem();
}

int
sys_setecho(void)
{
  int enable;
  if(argint(0, &enable) < 0)
    return -1;
  return setecho(enable);
}

int
sys_setuid(void)
{
  int uid;
  if(argint(0, &uid) < 0)
    return -1;
  if(uid < 0 || uid > ID_MAX)
    return -1;
  return procsetid(uid, 0);
}

int
sys_getuid(void)
{
  return myproc()->uid;
}

int
sys_forkjob(void)
{
  int mode;

  if(argint(0, &mode) < 0 || mode < JOB_FOREGROUND || mode > JOB_SESSION ||
     (mode == JOB_SESSION && myproc()->pid != 1))
    return -1;
  return forkin(mode);
}

static int
jobargs(int extended)
{
  int pid, mode, addr;
  char *status = 0;

  if(argint(0, &pid) < 0 || argint(1, &mode) < 0 || pid <= 0 ||
     (mode != JOB_POLL && mode != JOB_FOREGROUND && mode != JOB_RESUME &&
      !(extended && mode == JOB_CONT_BACKGROUND)))
    return -1;
  if(extended && (argint(2, &addr) < 0 ||
     (addr && argptrwrite(2, &status, sizeof(int)) < 0)))
    return -1;
  return jobctl(pid, mode, (int*)status);
}

int
sys_jobctl(void)
{
  return jobargs(0);
}

int
sys_jobctlx(void)
{
  return jobargs(1);
}

int
sys_sigaction(void)
{
  int sig, handler, trampoline;

  if(argint(0, &sig) < 0 || argint(1, &handler) < 0 ||
     argint(2, &trampoline) < 0)
    return -1;
  return setsignal(sig, handler, trampoline);
}

int
sys_sigreturn(void)
{
  return signalreturn();
}

int
sys_getgid(void)
{
  return myproc()->gid;
}

int
sys_setgid(void)
{
  int gid;

  if(argint(0, &gid) < 0 || gid < 0 || gid > ID_MAX)
    return -1;
  return procsetid(gid, 1);
}

int
sys_umask(void)
{
  struct proc *p = myproc();
  int mask, previous = p->umask;

  if(argint(0, &mask) < 0 || mask < -1 || mask > M_PERM)
    return -1;
  if(mask >= 0)
    p->umask = mask;
  return previous;
}

int
sys_getppid(void)
{
  return parentpid();
}

int
sys_waitpid(void)
{
  int pid, addr, options;
  char *status = 0;

  if(argint(0, &pid) < 0 || argint(1, &addr) < 0 ||
     argint(2, &options) < 0 || pid == (-2147483647 - 1) ||
     options & ~(WNOHANG | WUNTRACED | WCONTINUED) ||
     (addr && argptrwrite(1, &status, sizeof(int)) < 0))
    return -1;
  return procwait(pid, (int*)status, options);
}

int
sys_exitstatus(void)
{
  int status;

  if(argint(0, &status) < 0)
    return -1;
  procexit((status & 0xff) << 8);
}

int
sys_send_signal(void)
{
  int pid, sig;

  if(argint(0, &pid) < 0 || argint(1, &sig) < 0)
    return -1;
  return procsignal(pid, sig);
}

int
sys_getprocs(void)
{
  int n;
  char *data;

  if(argint(1, &n) < 0 || n < 1 || n > NPROC ||
     argptrwrite(0, &data, n * sizeof(struct procinfo)) < 0)
    return -1;
  return procsnapshot((struct procinfo*)data, n);
}

int
sys_memlimit(void)
{
  extern uint physlimit;
  return physlimit;
}

int
sys_cpuquery(void)
{
  int mode, count = 0;

  if(argint(0, &mode) < 0 || (mode != 0 && mode != 1 && mode != 3))
    return -1;
  if(mode == 0){
    pushcli();
    int cpu = cpuid();
    popcli();
    return cpu;
  }
  if(mode == 3)
    return sizeof(uptr) * 8;
  for(int i = 0; i < ncpu; i++)
    count += !!__atomic_load_n(&cpus[i].started, __ATOMIC_ACQUIRE);
  return count;
}

int
sys_usbquery(void)
{
  int op;

  if(argint(0, &op) < 0)
    return -1;
  if(op == 1)
    return usbdisks();
  if(op != 0 || myproc()->uid)
    return -1;
  return usbscan();
}
