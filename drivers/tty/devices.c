/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

static int
memory_read(struct inode *ip, char *dst, int n, uint off, int nonblock)
{
  if(ip->minor == MEMORY_NULL)
    return 0;
  if(ip->minor != MEMORY_ZERO)
    return -1;
  memset(dst, 0, n);
  return n;
}

static int
memory_write(struct inode *ip, char *src, int n, int nonblock)
{
  return ip->minor == MEMORY_NULL || ip->minor == MEMORY_ZERO ? n : -1;
}

void
memorydevinit(void)
{
  devsw[DEV_MEMORY].read = memory_read;
  devsw[DEV_MEMORY].write = memory_write;
}
