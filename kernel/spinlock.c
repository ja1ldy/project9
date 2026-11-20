/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
acquire(struct spinlock *lk)
{
  pushcli(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

  // The xchg is atomic.
  while(xchg(&lk->locked, 1) != 0)
    ;

  // tell the c compiler and the processor to not move loads or stores
  // past this point to ensure that the critical sections memory
  // references happen after the lock is acquired
  __sync_synchronize();

  // Record info about lock acquisition for debugging.
  lk->cpu = mycpu();
  getcallerpcs(__builtin_frame_address(0), lk->pcs);
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->pcs[0] = 0;
  lk->cpu = 0;

  // tell the c compiler and the processor to not move loads or stores
  // past this point to ensure that all the stores in the critical
  // section are visible to other cores before the lock is released
  // both the c compiler and the hardware may re order loads and
  // stores sync synchronize tells them both not to
  __sync_synchronize();

  // release the lock equivalent to lk to locked 0
  // this code cant use a c assignment since it might
  // not be atomic a real os would use c atomics here
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );

  popcli();
}

// Record the current call stack in pcs[] by following the %ebp chain.
void
getcallerpcs(void *v, uptr pcs[])
{
  uptr *ebp;
  uptr start = PGROUNDDOWN((uptr)v);
  int i;

  ebp = v;
  for(i = 0; i < 10; i++){
    if((uptr)ebp < KERNVIRT || (uptr)ebp < start ||
       (uptr)ebp - start > PGSIZE - 2*sizeof(uptr) || (uptr)ebp % sizeof(uptr))
      break;
    pcs[i] = ebp[1];     // saved %eip
    if(ebp[0] <= (uptr)ebp){
      i++;
      break;
    }
    ebp = (uptr*)ebp[0];
  }
  for(; i < 10; i++)
    pcs[i] = 0;
}

// Check whether this cpu is holding the lock.
int
holding(struct spinlock *lock)
{
  int r;
  pushcli();
  r = lock->locked && lock->cpu == mycpu();
  popcli();
  return r;
}


// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.

void
pushcli(void)
{
  int eflags;

  eflags = readeflags();
  cli();
  if(mycpu()->ncli == 0)
    mycpu()->intena = eflags & FL_IF;
  mycpu()->ncli += 1;
}

void
popcli(void)
{
  if(readeflags()&FL_IF)
    panic("popcli - interruptible");
  if(--mycpu()->ncli < 0)
    panic("popcli");
  if(mycpu()->ncli == 0 && mycpu()->intena)
    sti();
}

