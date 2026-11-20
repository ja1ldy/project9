/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "xstate.h"
#include "filemap.h"

#include "trapframe.h"
#include "signal.h"

#ifdef P9_KERNEL64
#define USER_NATIVE(tf) ((tf)->cs == ((SEG_UCODE64 << 3) | DPL_USER))
#else
#define USER_NATIVE(tf) 0
#endif

// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // set by the cpu when it is ready to run
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // interrupt state before the first pushcli
  struct proc *proc;           // current process if this cpu is running one
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// these are the registers swtch s leaves on the kernel stack
// the segment registers stay the same and the caller saves eax ecx
// and edx the call to swtch put eip on the stack already
struct context {
#ifdef P9_KERNEL64
  uptr r15, r14, r13, r12, ebx, ebp, eip;
#else
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
#endif
};

enum procstate { UNUSED, EMBRYO, SLEEPING, STOPPED, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  uint fbmaplen;
  struct filemap map[FILEMAP_COUNT];
  int fbmapwrite;
  uint sigmask, sigoldmask;
  uint handlermask[NSIG];
  int fpvalid;
  char fpstate[XSTATE_BYTES] __attribute__((aligned(XSTATE_ALIGN)));
  char fpsignal[XSTATE_BYTES] __attribute__((aligned(XSTATE_ALIGN)));
  uint heapbase;
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;
  int uid, gid;
  uint umask;
  int sid;
  int pgid;                    // terminal process group
  int tty_cbreak;
  struct file *ctty;
  uint pending;
  uint handlers[NSIG];
  int xstatus;
  int stop_notice, cont_notice;
  uint sigtramp;
  int insignal;
  struct trapframe sigframe;
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // signal forcing exit even during a handler
  uint closeexec;
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
