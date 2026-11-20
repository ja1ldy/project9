/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "ext2.h"
#include "kassert.h"
#include "procinfo.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "p9abi.h"
#include "tty.h"
#include "wait.h"
#include "usbhost.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;
static int ttyfg;

static uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);
static void sendsignal(struct proc*, int);

static int *
foreground_slot(struct proc *p)
{
  return p->ctty ? ptyforeground(p->ctty->pty) : &ttyfg;
}

static void
dropctty(void)
{
  struct proc *p = myproc();
  acquire(&ptable.lock);
  struct file *f = p->ctty;
  p->ctty = 0;
  p->tty_cbreak = 0;
  release(&ptable.lock);
  if(f)
    fileclose(f);
}
static int interrupted(struct proc*);
static void child_event(struct proc*);
static void stop_current(struct proc*, int);

#define STOP_MASK ((1U << SIGSTOP) | (1U << SIGTSTP) | (1U << SIGTTIN))

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// disable interrupts before calling so the lookup stays on this cpu
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // apic ids can have gaps so look for the matching cpu entry
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// keep the caller on this cpu while reading its current process
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// a recycled pid must not name another live or unreaped process
static int
allocatepid(void)
{
  for(;;){
    struct proc *p;
    int candidate = nextpid++;
    if(nextpid > 0x7fffffffU)
      nextpid = 2;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
      if(p->state != UNUSED && p->pid == candidate)
        break;
    if(p == &ptable.proc[NPROC])
      return candidate;
  }
}

void
setprocname(const char *name)
{
  struct proc *p = myproc();
  acquire(&ptable.lock);
  safestrcpy(p->name, name, sizeof p->name);
  release(&ptable.lock);
}

//PAGEBREAK: 32
// reserve a process slot and prepare its kernel stack
// return 0 when no slot or stack is available
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = allocatepid();
  p->pending = p->sigmask = p->sigoldmask = 0;
  p->fpvalid = 0;
  memset(p->handlermask, 0, sizeof(p->handlermask));
  p->fbmaplen = p->fbmapwrite = 0;
  private_reset(p);
  p->tty_cbreak = 0;
  p->ctty = 0;
  p->closeexec = 0;
  p->killed = p->xstatus = p->stop_notice = p->cont_notice = 0;
  p->insignal = 0;
  p->sigtramp = 0;
  memset(p->handlers, 0, sizeof(p->handlers));

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    acquire(&ptable.lock);
    p->pid = 0;
    p->state = UNUSED;
    release(&ptable.lock);
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= sizeof(uptr);
  *(uptr*)sp = (uptr)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uptr)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  KASSERT(p != 0);
  
  initproc = p;
  ttyfg = p->sid = p->pgid = p->pid;
  p->uid = p->gid = 0;
  p->umask = 0022;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (uptr)_binary_initcode_size);
  p->heapbase = p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));

  // publish the process under the table lock after its setup is complete
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  KASSERT(sz >= curproc->heapbase && sz < FILEMAP_BASE);
  if(n > 0){
    if((uint)n >= FILEMAP_BASE - sz)
      return -1;
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0){
      switchuvm(curproc);
      return -1;
    }
  } else if(n < 0){
    if(0U - (uint)n > sz - curproc->heapbase)
      return -1;
    sz = deallocuvm(curproc->pgdir, sz, sz + n);
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// copy the caller into a child and make it runnable
// an ordinary fork keeps the parents group a new job usually gets its own
int
forkin(int job)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // copy process state from proc
  np->pgdir = copyuvm(curproc->pgdir, curproc->sz);
  if(!np->pgdir || private_clone(np, curproc) < 0 || (curproc->fbmaplen &&
     fbmapuser(np->pgdir, curproc->fbmaplen, curproc->fbmapwrite) < 0)){
    if(np->pgdir){
      freevm(np->pgdir);
      np->pgdir = 0;
    }
    kfree(np->kstack);
    acquire(&ptable.lock);
    np->kstack = 0;
    np->pid = 0;
    np->state = UNUSED;
    release(&ptable.lock);
    return -1;
  }
  np->fbmaplen = curproc->fbmaplen;
  np->fbmapwrite = curproc->fbmapwrite;
  if(np->fbmapwrite)
    fbgraphics(1);
  np->sigmask = curproc->sigmask;
  fpusave(np->fpstate);
  np->fpvalid = 1;
  np->sz = curproc->sz;
  np->heapbase = curproc->heapbase;
  np->parent = curproc;
  np->uid = curproc->uid;
  np->gid = curproc->gid;
  np->umask = curproc->umask;
  np->pgid = job < 0 ? curproc->pgid : np->pid;
  np->sid = job == JOB_SESSION ? np->pid : curproc->sid;
  if(job < 0){
    memmove(np->handlers, curproc->handlers, sizeof(np->handlers));
    memmove(np->handlermask, curproc->handlermask, sizeof(np->handlermask));
    np->sigoldmask = curproc->sigoldmask;
    memmove(np->fpsignal, curproc->fpsignal, sizeof(np->fpsignal));
    np->sigtramp = curproc->sigtramp;
    np->insignal = curproc->insignal;
    np->sigframe = curproc->sigframe;
  }
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->closeexec = curproc->closeexec;
  np->cwd = idup(curproc->cwd);
  if(curproc->ctty)
    np->ctty = filedup(curproc->ctty);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  if(job == JOB_FOREGROUND || job == JOB_SESSION){
    int *foreground = foreground_slot(curproc);
    if(*foreground != curproc->pgid){
      // only the foreground group can hand the terminal to a new job
      np->pgid = curproc->pgid;
    } else {
      *foreground = np->pgid;
    }
  }
  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// fork a child in the callers process group
int
fork(void)
{
  return forkin(-1);
}

// keep an exited process in the table until its parent collects it
void
procexit(int status)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  fbunmapuser(curproc);
  ttyrestore();
  dropctty();

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  curproc->xstatus = curproc->killed ? curproc->killed : status;

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        child_event(p);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  child_event(curproc);
  sched();
  panic("zombie exit");
}

void
exit(void)
{
  procexit(0);
}

// process table readers use this same lock for credential checks
int
procsetid(int id, int group)
{
  struct proc *p = myproc();
  int result = -1;

  acquire(&ptable.lock);
  if(!p->uid || id == (group ? p->gid : p->uid)){
    if(group){
      p->gid = id;
    } else {
      if(id && !p->uid && !p->gid)
        p->gid = id;
      p->uid = id;
    }
    result = 0;
  }
  release(&ptable.lock);
  return result;
}

int
parentpid(void)
{
  struct proc *p = myproc();
  int pid;

  acquire(&ptable.lock);
  pid = p->parent ? p->parent->pid : 0;
  release(&ptable.lock);
  return pid;
}

static int
reap(struct proc *p)
{
  int pid = p->pid;

  KASSERT(holding(&ptable.lock) && p->state == ZOMBIE);
  kfree(p->kstack);
  p->kstack = 0;
  freevm(p->pgdir);
  p->pgdir = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->killed = 0;
  p->state = UNUSED;
  return pid;
}

// collect a matching child event a stopped child stays in the table
int
procwait(int selector, int *status, int options)
{
  struct proc *child, *self = myproc();
  int havekids, pid, code;

  acquire(&ptable.lock);
  for(;;){
    havekids = 0;
    for(child = ptable.proc; child < &ptable.proc[NPROC]; child++){
      if(child->parent != self ||
         (selector > 0 && child->pid != selector) ||
         (selector == 0 && child->pgid != self->pgid) ||
         (selector < -1 && child->pgid != -selector))
        continue;
      havekids = 1;
      pid = child->pid;
      if(child->state == ZOMBIE){
        code = child->xstatus;
        reap(child);
      } else if((options & WUNTRACED) && child->stop_notice){
        code = child->stop_notice << 8 | 0x7f;
        child->stop_notice = 0;
      } else if((options & WCONTINUED) && child->cont_notice){
        code = 0xffff;
        child->cont_notice = 0;
      } else {
        continue;
      }
      if(status)
        *status = code;
      release(&ptable.lock);
      return pid;
    }
    if(!havekids || interrupted(self)){
      release(&ptable.lock);
      return -1;
    }
    if(options & WNOHANG){
      release(&ptable.lock);
      return 0;
    }
    sleep(self, &ptable.lock);
  }
}

int
wait(void)
{
  return procwait(-1, 0, 0);
}

//PAGEBREAK: 42
// each cpu runs this loop after startup
// a runnable process takes over until it yields sleeps or exits
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    int ran = 0;

    cli();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      ran = 1;
      c->intena = 1;
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      if(p->fpvalid)
        fpurestore(p->fpstate);
      else
        fpureset();
      swtch(&(c->scheduler), p->context);
      fpusave(p->fpstate);
      p->fpvalid = 1;
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    c->intena = 0;
    release(&ptable.lock);

    // enable interrupts and sleep without a gap between the two instructions
    if(!ran)
      asm volatile("sti; hlt" : : : "memory");
    else
      sti();

  }
}

// set the process state and hold only ptable lock before calling sched
// save the interrupt state with this kernel thread so it follows the thread
// if the scheduler resumes it on another cpu
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// a new child starts here on its first turn through the scheduler
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
#ifdef P9_LEGACY_ROOT
    binit();
    legacydiskinit();
    iinit(ROOTDEV);
    initlog(ROOTDEV);
#else
    iinit();
#endif
    ext2init();
    myproc()->cwd = namei("/");
    ext2mountdata();
    usbinit();
    uhciinit();
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
static int
sleep1(void *chan, struct spinlock *lk, int interruptible)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // take ptable lock before dropping the callers lock wakeup also
  // takes ptable lock so it sees the process once it is ready to sleep
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  int interrupted_wait = 0;
  if(interruptible && interrupted(p)){
    interrupted_wait = 1;
  } else if(interruptible && (p->pending & ~p->sigmask & STOP_MASK)){
    int sig = __builtin_ctz(p->pending & ~p->sigmask & STOP_MASK);
    stop_current(p, sig);
  } else {
    p->chan = chan;
    p->state = SLEEPING;
    sched();
    p->chan = 0;
  }

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
  return interrupted_wait ? -1 : 0;
}

void
sleep(void *chan, struct spinlock *lk)
{
  sleep1(chan, lk, lk == &ptable.lock);
}

int
sleepintr(void *chan, struct spinlock *lk)
{
  return sleep1(chan, lk, 1);
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// the original kill pid syscall still means an unconditional kill
int
kill(int pid)
{
  return pid > 0 ? procsignal(pid, SIGKILL) : -1;
}

//PAGEBREAK: 36
// ctrl p prints a process listing for debugging
// skip the table lock so the listing can still help when a cpu is stuck
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [STOPPED]   "stop  ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uptr pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uptr*)p->context->ebp, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", (void*)pc[i]);
    }
    cprintf("\n");
  }
}

int
cps(void)
{
  struct proc *p;
  static char *states[] = {
    [UNUSED]    "unused",
    [EMBRYO]    "embryo",
    [SLEEPING]  "sleep ",
  [STOPPED]   "stop  ",
    [RUNNABLE]  "runble",
    [RUNNING]   "run   ",
    [ZOMBIE]    "zombie"
  };

  struct {
    int pid;
    enum procstate state;
    char name[sizeof(p->name)];
  } list[NPROC];
  int count = 0, i;

  // console interrupts take the console lock before the process table lock
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    list[count].pid = p->pid;
    list[count].state = p->state;
    safestrcpy(list[count].name, p->name, sizeof(list[count].name));
    count++;
  }
  release(&ptable.lock);

  cprintf("PID\tSTATE\tNAME\n");
  for(i = 0; i < count; i++)
    cprintf("%d\t%s\t%s\n", list[i].pid, states[list[i].state], list[i].name);
  return 22;
}

int
procsnapshot(struct procinfo *out, int limit)
{
  static const char states[] = {
    [EMBRYO] = 'I', [SLEEPING] = 'S', [STOPPED] = 'T',
    [RUNNABLE] = 'R', [RUNNING] = 'R', [ZOMBIE] = 'Z'
  };
  struct proc *p;
  int count = 0;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC] && count < limit; p++){
    if(p->state == UNUSED || p->state == EMBRYO)
      continue;
    memset(out, 0, sizeof(*out));
    out->pid = p->pid;
    out->ppid = p->parent ? p->parent->pid : 0;
    out->pgid = p->pgid;
    out->uid = p->uid;
    out->gid = p->gid;
    out->size = p->sz;
    out->state = states[p->state];
    safestrcpy(out->name, p->name, sizeof(out->name));
    out++;
    count++;
  }
  release(&ptable.lock);
  return count;
}

// terminal job control uses one foreground group for the system console
int
ttyforeground(void)
{
  int yes;
  struct proc *p = myproc();
  acquire(&ptable.lock);
  yes = p && !p->ctty && p->pgid == ttyfg;
  release(&ptable.lock);
  return yes;
}

// a stopped or background reader gives the console back to the shell
int
ttyinputowner(struct pty *terminal)
{
  struct proc *p;
  int pid = 0;

  acquire(&ptable.lock);
  int foreground = terminal ? *ptyforeground(terminal) : ttyfg;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->tty_cbreak && (p->ctty ? p->ctty->pty : 0) == terminal && p->pgid == foreground &&
       (p->state == SLEEPING || p->state == RUNNABLE || p->state == RUNNING)){
      pid = p->pid;
      break;
    }
  }
  release(&ptable.lock);
  return pid;
}

int
ttysetcbreak(struct pty *terminal, int enable)
{
  struct proc *p = myproc();
  int result = -1;

  acquire(&ptable.lock);
  int foreground = terminal ? *ptyforeground(terminal) : ttyfg;
  if((p->ctty ? p->ctty->pty : 0) == terminal && (!enable || p->pgid == foreground)){
    p->tty_cbreak = enable;
    result = 0;
  }
  release(&ptable.lock);
  return result;
}

static int
validsignal(int sig)
{
  return sig == SIGHUP || sig == SIGINT || sig == SIGQUIT || sig == SIGKILL ||
         sig == SIGSEGV || sig == SIGPIPE || sig == SIGTERM || sig == SIGCHLD ||
         sig == SIGCONT || sig == SIGSTOP || sig == SIGTSTP || sig == SIGTTIN || sig == SIGWINCH;
}

// a caught signal waits until the current handler returns stops are
// serviced at the next safe point even when a handler is running
static int
nextsignal(struct proc *p)
{
  int sig;
  for(sig = 1; sig < NSIG; sig++)
    if(((p->pending & ~p->sigmask) & (1U << sig)) && (!p->insignal || p->handlers[sig] == 0))
      return sig;
  return 0;
}

static int
interrupted(struct proc *p)
{
  int sig = nextsignal(p);
  return p->killed || (sig && !((1U << sig) & STOP_MASK));
}

static void
child_event(struct proc *p)
{
  if(p->parent){
    wakeup1(p->parent);
    sendsignal(p->parent, SIGCHLD);
  }
  wakeup1(&ttyfg);
}

static void
stop_current(struct proc *p, int sig)
{
  p->pending &= ~STOP_MASK;
  p->state = STOPPED;
  p->stop_notice = sig;
  p->cont_notice = 0;
  child_event(p);
  sched();
}

static void
resume_locked(struct proc *p)
{
  p->pending &= ~STOP_MASK;
  if(p->state == STOPPED){
    p->state = RUNNABLE;
    p->stop_notice = 0;
    p->cont_notice = 1;
    child_event(p);
  }
}

// hold ptable lock while queuing a signal or changing a childs state
static void
sendsignal(struct proc *p, int sig)
{
  if(p == initproc || p->state < SLEEPING || p->state == ZOMBIE)
    return;
  if(sig == SIGKILL){
    p->killed = SIGKILL;
    if(p->state == SLEEPING || p->state == STOPPED)
      p->state = RUNNABLE;
    return;
  }
  if(sig == SIGCONT)
    resume_locked(p);
  if(p->handlers[sig] == 1 || ((sig == SIGCHLD || sig == SIGCONT || sig == SIGWINCH) && !p->handlers[sig]))
    return;
  if((1U << sig) & STOP_MASK)
    p->pending &= ~(1U << SIGCONT);
  p->pending |= 1U << sig;
  if(p->state == SLEEPING &&
     (interrupted(p) || (p->pending & ~p->sigmask & STOP_MASK)))
    p->state = RUNNABLE;
}

int
procsignal(int selector, int sig)
{
  struct proc *p, *self = myproc();
  int result = -1;

  if((sig && !validsignal(sig)) || selector == (-2147483647 - 1))
    return -1;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state < SLEEPING || p->state == ZOMBIE ||
       (p == initproc && sig) ||
       (self->uid != 0 && p->uid != self->uid &&
        (sig != SIGCONT || p->sid != self->sid)))
      continue;
    if((selector > 0 && p->pid != selector) ||
       (selector == 0 && p->pgid != self->pgid) ||
       (selector == -1 && p == self) ||
       (selector < -1 && p->pgid != -selector))
      continue;
    if(sig)
      sendsignal(p, sig);
    result = 0;
  }
  release(&ptable.lock);
  return result;
}

// queue the signal for each member of the foreground group
void
ttysignal(int sig)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(!p->ctty && p->pgid == ttyfg)
      sendsignal(p, sig);
  release(&ptable.lock);
}

int
ptyassign(struct file *f)
{
  struct proc *p = myproc();
  if(p->sid != p->pid || p->ctty)
    return -1;
  struct file *ref = filedup(f);
  acquire(&ptable.lock);
  p->ctty = ref;
  *ptyforeground(f->pty) = p->pgid;
  release(&ptable.lock);
  return 0;
}

void
ptysignal(struct pty *pty, int sig)
{
  acquire(&ptable.lock);
  int group = *ptyforeground(pty);
  for(struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state >= SLEEPING && p->state != ZOMBIE &&
       p->ctty && p->ctty->pty == pty && (sig == SIGHUP || p->pgid == group)){
      if(sig == SIGHUP)
        sendsignal(p, SIGCONT);
      sendsignal(p, sig);
    }
  }
  release(&ptable.lock);
}

int
ptyreadcheck(struct pty *pty)
{
  struct proc *self = myproc();
  int result = 0;
  acquire(&ptable.lock);
  if(self->ctty && self->ctty->pty == pty && *ptyforeground(pty) != self->pgid){
    result = -P9_EIO;
    if(!self->handlers[SIGTTIN] && !(self->sigmask & (1U << SIGTTIN))){
      for(struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if(p->state >= SLEEPING && p->state != ZOMBIE &&
           p->ctty && p->ctty->pty == pty && p->pgid == self->pgid)
          sendsignal(p, SIGTTIN);
      result = -P9_EINTR;
    }
  }
  release(&ptable.lock);
  return result;
}

// poll or wait for a child job job resume also puts it in the foreground
int
jobctl(int pid, int mode, int *status)
{
  struct proc *p, *leader, *self = myproc();
  int active, stopped;
  int group;

  acquire(&ptable.lock);
  int *foreground = foreground_slot(self);
  for(leader = ptable.proc; leader < &ptable.proc[NPROC]; leader++)
    if(leader->pid == pid && leader->parent == self)
      break;
  if(leader == &ptable.proc[NPROC]){
    release(&ptable.lock);
    return -1;
  }
  group = leader->pgid;
  if(group != self->pgid &&
     (mode == JOB_CONT_BACKGROUND || (mode == JOB_RESUME && *foreground == self->pgid))){
    if(mode == JOB_RESUME)
      *foreground = group;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->pgid != group || p->state == UNUSED)
        continue;
      sendsignal(p, SIGCONT);
    }
  }
  for(;;){
    if(self == initproc){
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if(p != leader && p->parent == self && p->state == ZOMBIE)
          reap(p);
    }
    // end the rest of the login session before bringing back the prompt
    if(self == initproc && leader->state == ZOMBIE){
      *foreground = self->pgid;
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p == leader || p->state < SLEEPING || p->state == ZOMBIE ||
           p->sid != leader->sid)
          continue;
        sendsignal(p, SIGKILL);
      }
    }
    active = stopped = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if((self == initproc ? p->sid != leader->sid :
          group == self->pgid ? p != leader : p->pgid != group) ||
         p->state == UNUSED || p->state == ZOMBIE)
        continue;
      if(p->state == STOPPED)
        stopped++;
      else
        active++;
    }
    // the shell can take the terminal back when the whole job stops or exits
    if(active == 0){
      if(mode != JOB_POLL && mode != JOB_CONT_BACKGROUND && *foreground == group)
        *foreground = self->pgid;
      if(stopped == 0){
        if(status)
          *status = leader->xstatus;
        reap(leader);
      }
      release(&ptable.lock);
      return stopped ? JOB_STOPPED : 0;
    }
    if(mode == JOB_POLL || mode == JOB_CONT_BACKGROUND || interrupted(self)){
      if((mode == JOB_FOREGROUND || mode == JOB_RESUME) && *foreground == group)
        *foreground = self->pgid;
      release(&ptable.lock);
      return JOB_RUNNING;
    }
    sleep(&ttyfg, &ptable.lock);
  }
}

int
setsignalx(int sig, uint handler, uint trampoline, uint mask, uint *oldmask)
{
  struct proc *p = myproc();
  int old;

  if(!validsignal(sig) ||
     (handler != ~0U && (sig == SIGKILL || sig == SIGSTOP ||
      (sig == SIGTSTP && handler > 1) ||
      (handler > 1 && (handler >= p->sz || !trampoline || trampoline >= p->sz ||
                       !userpage(p->pgdir, handler, 0) ||
                       !userpage(p->pgdir, trampoline, 0))))))
    return -1;
  acquire(&ptable.lock);
  old = p->handlers[sig];
  if(oldmask)
    *oldmask = p->handlermask[sig];
  if(handler == ~0U){
    release(&ptable.lock);
    return old;
  }
  p->handlers[sig] = handler;
  p->handlermask[sig] = mask & ~((1U << SIGKILL) | (1U << SIGSTOP));
  if(handler > 1)
    p->sigtramp = trampoline;
  if(handler == 1 || (!handler && (sig == SIGCHLD || sig == SIGCONT)))
    p->pending &= ~(1U << sig);
  release(&ptable.lock);
  return old;
}

int
setsignal(int sig, uint handler, uint trampoline)
{
  if(handler == ~0U)
    return -1;
  return setsignalx(sig, handler, trampoline, 0, 0);
}

void
fpuexec(void)
{
  fpureset();
}

void
signalexec(void)
{
  struct proc *p = myproc();
  int i;

  acquire(&ptable.lock);
  p->insignal = 0;
  for(i = 1; i < NSIG; i++){
    if(p->handlers[i] > 1)
      p->handlers[i] = 0;
  }
  if(!p->handlers[SIGCHLD])
    p->pending &= ~(1U << SIGCHLD);
  if(!p->handlers[SIGCONT])
    p->pending &= ~(1U << SIGCONT);
  release(&ptable.lock);
}

int
signalreturn(void)
{
  struct proc *p = myproc();

  if(!p->insignal)
    return -1;
  acquire(&ptable.lock);
  *p->tf = p->sigframe;
  p->sigmask = p->sigoldmask;
  p->insignal = 0;
  release(&ptable.lock);
  fpurestore(p->fpsignal);
  return 0;
}

void
signaldeliver(struct trapframe *tf)
{
  struct proc *p = myproc();
  uint handler, frame[2], sp;
  int sig;

again:
  if(p->killed)
    exit();
  acquire(&ptable.lock);
  sig = nextsignal(p);
  if(!sig){
    release(&ptable.lock);
    return;
  }
  handler = p->handlers[sig];
  if(((1U << sig) & STOP_MASK) && handler == 0){
    stop_current(p, sig);
    release(&ptable.lock);
    goto again;
  }
  p->pending &= ~(1U << sig);
  release(&ptable.lock);
  if(handler == 1 || (!handler && (sig == SIGCHLD || sig == SIGCONT)))
    goto again;
  if(handler == 0){
    int status = sig;
    if(sig == SIGQUIT && coredump(tf))
      status |= 0x80;
    procexit(status);
  }
  // enter the handler as if user code had called it its return path
  // restores the saved registers and resumes the interrupted code
#ifdef P9_KERNEL64
  int native = USER_NATIVE(tf);
  if(native){
    uptr trampoline = p->sigtramp;
    if(tf->esp < 144 || tf->esp > p->sz)
      procexit(SIGSEGV);
    sp = ((tf->esp - 128) & ~15UL) - sizeof(uptr);
    if(copyout(p->pgdir, sp, &trampoline, sizeof(trampoline)) < 0)
      procexit(SIGSEGV);
  } else
#endif
  {
    frame[0] = p->sigtramp;
    frame[1] = sig;
    if(tf->esp < sizeof(frame) || tf->esp > p->sz)
      procexit(SIGSEGV);
    sp = tf->esp - sizeof(frame);
    if(copyout(p->pgdir, sp, frame, sizeof(frame)) < 0)
      procexit(SIGSEGV);
  }
  p->sigframe = *tf;
  fpusave(p->fpsignal);
  acquire(&ptable.lock);
  p->sigoldmask = p->sigmask;
  p->sigmask |= p->handlermask[sig] | (1U << sig);
  p->insignal = 1;
  release(&ptable.lock);
#ifdef P9_KERNEL64
  if(native)
    tf->edi = sig;
#endif
  tf->esp = sp;
  tf->eip = handler;
}

int
newsession(void)
{
  struct proc *p = myproc();
  int result = -1;

  acquire(&ptable.lock);
  if(p->pgid != p->pid){
    p->pgid = p->sid = p->pid;
    result = p->pid;
  }
  release(&ptable.lock);
  if(result > 0)
    dropctty();
  return result;
}

uint
signalmask(int mode, uint mask)
{
  struct proc *p = myproc();
  uint old;

  mask &= ((1U << NSIG) - 2) & ~((1U << SIGKILL) | (1U << SIGSTOP));
  acquire(&ptable.lock);
  old = p->sigmask;
  if(mode == 0)
    p->sigmask |= mask;
  if(mode == 1)
    p->sigmask &= ~mask;
  if(mode == 2)
    p->sigmask = mask;
  release(&ptable.lock);
  return old;
}
