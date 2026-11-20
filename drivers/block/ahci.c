/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "disk.h"
#include "memlayout.h"
#include "pci.h"

#define MAX_PORTS 32
#define WAIT 3000000
#define SECTOR 512
#define START (1U << 0)
#define RECEIVE (1U << 4)
#define RECEIVER_RUNNING (1U << 14)
#define COMMAND_RUNNING (1U << 15)
#define FATAL (0x7dU << 24)

struct dma {
  uint headers[256];
  uchar fis[256];
  uchar command[128];
  uint prdt[4];
  uchar reserved[624];
  uchar data[AHCI_BATCH*SECTOR];
};

_Static_assert(sizeof(struct dma) <= 4096, "ahci dma area");
_Static_assert(__builtin_offsetof(struct dma, command) == 1280, "ahci command alignment");
_Static_assert(__builtin_offsetof(struct dma, prdt) == 1408, "ahci prdt offset");

struct port {
  volatile uint *reg;
  struct dma *dma;
  uint sectors;
  int lba48, failed;
  uchar flush_command;
};

static struct port ports[MAX_PORTS];
static uint count;

static int
wait_clear(volatile uint *reg, uint mask)
{
  uint i;

  for(i = 0; i < WAIT; i++)
    if(!(*reg & mask))
      return 0;
  return -1;
}

void
ahcifound(const struct pci_device *device)
{
  volatile uint *hba, *reg;
  uint bar, implemented, i;

  bar = pciread(device, 0x24);
  if(!bar || bar == ~0U || (bar & 7))
    return;
  hba = iomap(bar & ~15U, 0x1100);
  if(!hba)
    return;
  pci_enable(device, 6);
  if(hba[0x24/4] & 1){
    hba[0x28/4] |= 2;
    if(wait_clear(&hba[0x28/4], 0x11) < 0)
      return;
  }
  hba[4/4] = (hba[4/4] & ~2U) | (1U << 31);
  implemented = hba[0xc/4];
  for(i = 0; i < 32 && count < MAX_PORTS; i++){
    if(!(implemented & (1U << i)))
      continue;
    reg = hba + (0x100 + 0x80*i)/4;
    if((reg[0x28/4] & 0xf0f) == 0x103 && reg[0x24/4] == 0x101)
      ports[count++].reg = reg;
  }
}

uint
ahcidisks(void)
{
  return count;
}

static int
submit(struct port *p, uchar code, uint lba, uint sectors, int write)
{
  if(!p->dma || p->failed || sectors > AHCI_BATCH)
    return -1;
  struct dma *d = p->dma;
  volatile uint *r = p->reg;
  uchar *fis = d->command;
  uint i;

  if(wait_clear(&r[0x20/4], 0x88) < 0 ||
     wait_clear(&r[0x38/4], 1) < 0)
    goto failed;
  memset(fis, 0, sizeof(d->command));
  fis[0] = 0x27;
  fis[1] = 0x80;
  fis[2] = code;
  if(code != 0xec && sectors){
    fis[4] = lba;
    fis[5] = lba >> 8;
    fis[6] = lba >> 16;
    fis[7] = 0x40;
    if(p->lba48)
      fis[8] = lba >> 24;
    else
      fis[7] |= (lba >> 24) & 15;
    fis[12] = sectors;
  }
  d->headers[0] = 5 | (write ? 0x40 : 0) | (sectors ? 1U << 16 : 0);
  d->headers[1] = 0;
  d->headers[2] = V2P(fis);
  d->prdt[0] = V2P(d->data);
  d->prdt[3] = sectors ? sectors*SECTOR - 1 : 0;
  r[0x10/4] = ~0U;
  __sync_synchronize();
  r[0x38/4] = 1;
  for(i = 0; i < WAIT; i++){
    if(r[0x10/4] & FATAL)
      goto failed;
    if(!(r[0x38/4] & 1)){
      __sync_synchronize();
      if(!(r[0x20/4] & 0x89) && *(volatile uint *)&d->headers[1] == sectors*SECTOR)
        return 0;
      goto failed;
    }
  }
failed:
  // a timed out controller may still own the dma page
  p->failed = 1;
  r[0x18/4] &= ~START;
  if(wait_clear(&r[0x18/4], COMMAND_RUNNING) == 0){
    r[0x18/4] &= ~RECEIVE;
    wait_clear(&r[0x18/4], RECEIVER_RUNNING);
  }
  return -1;
}

uint
ahciprobe(uint index)
{
  if(index >= count)
    return 0;
  struct port *p = &ports[index];
  volatile uint *r = p->reg;
  ushort *identify;

  if(p->failed)
    return 0;
  if(p->dma)
    return p->sectors;
  r[0x18/4] &= ~START;
  if(wait_clear(&r[0x18/4], COMMAND_RUNNING) < 0)
    return 0;
  r[0x18/4] &= ~RECEIVE;
  if(wait_clear(&r[0x18/4], RECEIVER_RUNNING) < 0)
    return 0;
  p->dma = (struct dma*)kalloc();
  if(!p->dma)
    return 0;
  memset(p->dma, 0, sizeof(*p->dma));
  r[0] = V2P(p->dma->headers);
  r[1] = 0;
  r[2] = V2P(p->dma->fis);
  r[3] = 0;
  r[0x14/4] = 0;
  r[0x30/4] = ~0U;
  r[0x18/4] |= RECEIVE;
  r[0x18/4] |= START;
  if(submit(p, 0xec, 0, 1, 0) < 0)
    return 0;
  identify = (ushort*)p->dma->data;
  if(!(identify[49] & (1 << 9)) ||
     ((identify[106] & 0xd000) == 0x5000 && (identify[117] != 256 || identify[118])))
    return 0;
  p->lba48 = !!(identify[83] & (1 << 10));
  p->flush_command = (identify[83] & 0xe000) == 0x6000 ? 0xea : 0xe7;
  if(p->lba48)
    p->sectors = identify[102] || identify[103] ? ~0U :
                 (uint)identify[100] | (uint)identify[101] << 16;
  else
    p->sectors = (uint)identify[60] | (uint)identify[61] << 16;
  return p->sectors;
}

int
ahcitransfer(uint index, uint lba, uint n, void *buf, int write)
{
  if(index >= count)
    return -1;
  struct port *p = &ports[index];
  uchar code;

  if(p->failed || !p->dma || !n || n > AHCI_BATCH || lba >= p->sectors || n > p->sectors-lba)
    return -1;
  if(write)
    memmove(p->dma->data, buf, n*SECTOR);
  code = p->lba48 ? (write ? 0x35 : 0x25) : (write ? 0xca : 0xc8);
  if(submit(p, code, lba, n, write) < 0)
    return -1;
  if(!write)
    memmove(buf, p->dma->data, n*SECTOR);
  return 0;
}

int
ahciflush(uint index)
{
  if(index >= count)
    return -1;
  struct port *p = &ports[index];

  return submit(p, p->flush_command, 0, 0, 0);
}
