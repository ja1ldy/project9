/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "buf.h"
#include "ext2.h"

static struct sleeplock disklock;

void
legacydiskinit(void)
{
  initsleeplock(&disklock, "legacy disk");
  if(!ataextprobe(ROOTDEV))
    panic("no root disk for xv6 filesystem");
}

void
iderw(struct buf *b)
{
  if(b->dev != ROOTDEV || !holdingsleep(&b->lock))
    panic("bad legacy disk request");
  acquiresleep(&disklock);
  if(ataextrw(ROOTDEV, b->blockno, b->data, b->flags & B_DIRTY) < 0)
    panic("xv6 root disk io error");
  b->flags = (b->flags & ~B_DIRTY) | B_VALID;
  releasesleep(&disklock);
}
