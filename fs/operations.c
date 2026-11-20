/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "ext2.h"
#include "spinlock.h"
#include "kassert.h"
#include "p9abi.h"

static struct spinlock lock;
static uint active;
static int freezing;

void
fsopsinit(void)
{
  initlock(&lock, "filesystem operations");
}

void
begin_op(void)
{
  acquire(&lock);
  while(freezing)
    sleep(&active, &lock);
  active++;
  release(&lock);
}

void
end_op(void)
{
  acquire(&lock);
  KASSERT(active > 0);
  if(--active == 0)
    wakeup(&active);
  release(&lock);
}

int
fsfreeze(void)
{
  int result;

  acquire(&lock);
  if(freezing){
    release(&lock);
    return -P9_EBUSY;
  }
  freezing = 1;
  while(active)
    sleep(&active, &lock);
  release(&lock);

  // keep the barrier up after a successful sync until power is cut
  result = ext2sync();
  if(result < 0)
    fsrelease();
  return result;
}

void
fsrelease(void)
{
  acquire(&lock);
  freezing = 0;
  wakeup(&active);
  release(&lock);
}
