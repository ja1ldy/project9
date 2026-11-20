/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "stat.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "core.h"
#include "tty.h"

// write the trap frame and as much user memory as one xv6 file can hold
int
coredump(struct trapframe *tf)
{
  struct proc *p = myproc();
  struct core_header header;
  struct inode *ip;
  struct file out;
  uint va = 0, n, pid = p->pid;
  char *page;
  char path[sizeof("core.2147483647")];
  int i = sizeof(path) - 1;
  static char zero[512];

  path[i] = 0;
  do {
    path[--i] = '0' + pid % 10;
    pid /= 10;
  } while(pid && i > 5);
  i -= 5;
  memmove(path+i, "core.", 5);

  begin_op();
  ip = create(path+i, T_FILE, p->uid, p->gid, 0600, 1, 0);
  if(ip)
    iunlock(ip);
  end_op();
  if(!ip)
    return 0;

  memset(&out, 0, sizeof(out));
  out.ip = ip;
  out.type = FD_INODE;
  out.writable = 1;

  memset(&header, 0, sizeof(header));
  header.magic = CORE_MAGIC;
  header.version = CORE_VERSION;
  header.pid = p->pid;
  header.uid = p->uid;
  header.signal = SIGQUIT;
  header.memory_size = p->sz;
  header.saved_size = 64 * 1024 - sizeof(header);
  if(header.saved_size > p->sz)
    header.saved_size = p->sz;
#ifdef P9_KERNEL64
  header.registers = (struct trapframe32){
    .edi = tf->edi, .esi = tf->esi, .ebp = tf->ebp, .ebx = tf->ebx,
    .eax = tf->eax, .ecx = tf->ecx, .edx = tf->edx,
    .gs = tf->gs, .fs = tf->fs, .es = tf->es, .ds = tf->ds,
    .eip = tf->eip, .cs = tf->cs, .eflags = tf->eflags,
    .esp = tf->esp, .ss = tf->ss, .trapno = tf->trapno, .err = tf->err
  };
#else
  header.registers = *tf;
#endif
  if(filewrite(&out, (char*)&header, sizeof(header)) != sizeof(header))
    goto done;

  for(va = 0; va < header.saved_size; va += n){
    // leave gaps in the address space zeroed so offsets still line up
    page = uva2ka(p->pgdir, (char*)(uptr)PGROUNDDOWN(va));
    n = BSIZE;
    if(n > header.saved_size - va)
      n = header.saved_size - va;
    if(filewrite(&out, page ? page + (va % PGSIZE) : zero, n) != n)
      break;
  }

done:
  begin_op();
  iput(ip);
  end_op();
  if(va == header.saved_size)
    cprintf("pid %d: SIGQUIT (core written to %s)\n", p->pid, path+i);
  else
    cprintf("pid %d: SIGQUIT (incomplete core %s)\n", p->pid, path+i);
  return va == header.saved_size;
}
