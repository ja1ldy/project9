/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uptr vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(!myproc()->killed)
      signaldeliver(tf);
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
      uhci_tick();
    }
    irqeoi(tf->trapno - T_IRQ0);
    break;
  case T_IRQ0 + IRQ_IDE:
  case T_IRQ0 + IRQ_IDE+1:
    irqeoi(tf->trapno - T_IRQ0);
    break;
  case T_IRQ0 + IRQ_AUX:
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    irqeoi(tf->trapno - T_IRQ0);
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    irqeoi(tf->trapno - T_IRQ0);
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), (uint)tf->cs, (uint)tf->eip);
    irqeoi(tf->trapno - T_IRQ0);
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0)
      panictrap(tf);
    cprintf("pid %d %s: trap %d err %d on cpu %d eip 0x%x esp 0x%x",
            myproc()->pid, myproc()->name, (int)tf->trapno, (int)tf->err,
            cpuid(), (uint)tf->eip, (uint)tf->esp);
    if(tf->trapno == T_PGFLT)
      cprintf(" addr 0x%p", (void*)rcr2());
    cprintf(" -- kill proc\n");
    procexit(SIGSEGV);
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
  if(myproc() && (tf->cs&3) == DPL_USER)
    signaldeliver(tf);
}
