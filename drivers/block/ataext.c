/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "ext2.h"
#include "disk.h"
#include "p9abi.h"
#include "spinlock.h"
#include "x86.h"

#define ATA_BUSY 0x80
#define ATA_READY 0x40
#define ATA_FAULT 0x20
#define ATA_DRQ 0x08
#define ATA_ERR 0x01
#define ATA_IDENTIFY 0xec
#define ATA_READ 0x20
#define ATA_WRITE 0x30
#define ATA_FLUSH 0xe7
#define ATA_LBA28 0x10000000U
#define ATA_WAIT 1000000
#define ATA_FLUSH_TICKS (30 * P9_HZ)

// the disk layer serializes access to each channel
static const struct channel {
  ushort io, control;
  uchar slave;
} channel[] = {
  [0]       = {0x1f0, 0x3f6, 0},
  [ROOTDEV] = {0x1f0, 0x3f6, 0x10},
  [EXT2DEV] = {0x170, 0x376, 0},
  [3]       = {0x170, 0x376, 0x10},
};
static uint capacity[NELEM(channel)];
static uchar failed[NELEM(channel)/2];

static void
delay(const struct channel *c)
{
  int i;
  for(i = 0; i < 4; i++)
    inb(c->control);
}

static int
waitfor(const struct channel *c, int bits, int check)
{
  int i, status;

  for(i = 0; i < ATA_WAIT; i++){
    status = inb(c->io+7);
    if(status == 0 || status == 0xff)
      return -1;
    if(status & ATA_BUSY)
      continue;
    if(check && (status & (ATA_ERR | ATA_FAULT)))
      return -1;
    if((status & bits) == bits && ((status ^ bits) & ATA_DRQ) == 0)
      return 0;
  }
  return -1;
}

uint
ataextprobe(uint dev)
{
  if(dev >= NELEM(channel))
    return 0;
  const struct channel *c = &channel[dev];
  ushort id[256];
  uint i;

  outb(c->control, 2);
  outb(c->io+6, 0xa0 | c->slave);
  delay(c);
  if(waitfor(c, ATA_READY, 0) < 0)
    return 0;
  outb(c->io+2, 0);
  outb(c->io+3, 0);
  outb(c->io+4, 0);
  outb(c->io+5, 0);
  outb(c->io+7, ATA_IDENTIFY);
  delay(c);
  if(waitfor(c, ATA_DRQ, 1) < 0)
    return 0;
  for(i = 0; i < 256; i++)
    id[i] = inw(c->io);
  if(!(id[49] & (1 << 9)))
    return 0;
  // ext2 uses 512 byte logical sectors on this controller
  if((id[106] & 0xd000) == 0x5000 && (id[117] != 256 || id[118]))
    return 0;
  capacity[dev] = (uint)id[60] | (uint)id[61] << 16;
  if(capacity[dev] > ATA_LBA28)
    capacity[dev] = ATA_LBA28;
  return capacity[dev];
}

static int
transfer(uint dev, uint lba, uint count, void *buffer, int writing)
{
  const struct channel *c = &channel[dev];
  uchar *data = buffer;
  ushort port = c->io;

  outb(c->io+6, 0xe0 | c->slave | (lba >> 24));
  delay(c);
  if(waitfor(c, ATA_READY, 0) < 0)
    return -1;
  outb(c->io+2, count);
  outb(c->io+3, lba);
  outb(c->io+4, lba >> 8);
  outb(c->io+5, lba >> 16);
  outb(c->io+7, writing ? ATA_WRITE : ATA_READ);
  delay(c);
  while(count--){
    uint words = 256;
    void *sector = data;
    if(waitfor(c, ATA_DRQ, 1) < 0)
      return -1;
    if(writing)
      asm volatile("cld; rep outsw" : "+S"(sector), "+c"(words) : "d"(port) : "memory");
    else
      asm volatile("cld; rep insw" : "+D"(sector), "+c"(words) : "d"(port) : "memory");
    delay(c);
    data += 512;
  }
  return waitfor(c, ATA_READY, 1);
}

static int
waitflush(uint dev, const struct channel *c)
{
  uint start;
  int status, timeout;

  // a busy disk can take seconds to commit its cache
  acquire(&tickslock);
  start = ticks;
  for(;;){
    status = inb(c->io+7);
    if(status == 0 || status == 0xff)
      break;
    if(!(status & ATA_BUSY)){
      if(status & (ATA_ERR | ATA_FAULT))
        break;
      if((status & (ATA_READY | ATA_DRQ)) == ATA_READY){
        release(&tickslock);
        return 0;
      }
    }
    if(ticks-start >= ATA_FLUSH_TICKS)
      break;
    sleep(&ticks, &tickslock);
  }
  timeout = ticks-start >= ATA_FLUSH_TICKS;
  release(&tickslock);
  cprintf("ata: disk %d cache flush %s status 0x%x\n",
          dev, timeout ? "timed out" : "failed", status);
  return -1;
}

int
ataextflush(uint dev)
{
  if(dev >= NELEM(channel) || !capacity[dev] || failed[dev/2])
    return -1;
  const struct channel *c = &channel[dev];

  outb(c->io+6, 0xe0 | c->slave);
  delay(c);
  if(waitfor(c, ATA_READY, 1) == 0){
    outb(c->io+7, ATA_FLUSH);
    delay(c);
    if(waitflush(dev, c) == 0)
      return 0;
  } else
    cprintf("ata: disk %d not ready for cache flush status 0x%x\n", dev, inb(c->io+7));
  failed[dev/2] = 1;
  return -1;
}

int
ataexttransfer(uint dev, uint lba, uint count, void *buffer, int writing)
{
  if(dev >= NELEM(channel) || failed[dev/2] || !count || count > DISK_BATCH ||
     lba >= capacity[dev] || count > capacity[dev]-lba)
    return -1;
  if(transfer(dev, lba, count, buffer, writing) == 0)
    return 0;
  failed[dev/2] = 1;
  return -1;
}

// the legacy journal uses synchronous sector writes
int
ataextrw(uint dev, uint lba, void *buffer, int writing)
{
  if(dev >= NELEM(channel) || failed[dev/2])
    return -1;
  const struct channel *c = &channel[dev];

  if(lba >= capacity[dev])
    return -1;
  if(transfer(dev, lba, 1, buffer, writing) == 0)
    return writing ? ataextflush(dev) : 0;

  // reset a stalled channel once and retry the same sector
  outb(c->control, 6);
  for(int i = 0; i < 16; i++)
    delay(c);
  outb(c->control, 2);
  delay(c);
  if(waitfor(c, 0, 0) < 0)
    return -1;
  if(transfer(dev, lba, 1, buffer, writing) < 0)
    return -1;
  return writing ? ataextflush(dev) : 0;
}
