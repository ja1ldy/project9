/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "pci.h"
#include "usbhost.h"

enum {
  COMMAND = 0, STATUS = 1, INTR = 2, SEGMENT = 4, ASYNC = 6,
  CONFIG = 16, PORT = 17
};

#define RUN 1
#define RESET 2
#define ASYNC_ENABLE 32
#define ASYNC_STATUS (1U << 15)
#define HALTED (1U << 12)
#define PORT_CHANGES 0x2a

struct ehci_dma {
  // leave room for all five high dma address words in each descriptor
  volatile uint qh[12], qh_high[5];
  uint qh_padding[7];
  volatile uint td[8], td_high[5];
  uint td_padding[3];
  uchar data[512];
};

_Static_assert(sizeof(struct ehci_dma) <= 4096, "ehci dma fits a page");
_Static_assert(__builtin_offsetof(struct ehci_dma, td) % 32 == 0, "ehci td alignment");
_Static_assert(__builtin_offsetof(struct ehci_dma, qh_high) == 48, "ehci qh high addresses");
_Static_assert(__builtin_offsetof(struct ehci_dma, td_high) ==
               __builtin_offsetof(struct ehci_dma, td) + 32, "ehci td high addresses");

static int
wait_clear(volatile uint *reg, uint bits)
{
  for(int i = 0; i < 50; i++){
    if(!(*reg & bits))
      return 0;
    usbwait(1);
  }
  return -1;
}

static int
start(struct usbhost *h)
{
  volatile uint *r = h->regs;
  struct pci_device dev = {h->pci_address};
  uint legacy = h->legacy;

  if(legacy >= 0x40 && legacy <= 0xf8 && !(legacy & 3) &&
     (pciread(&dev, legacy) & 255) == 1){
    pci_write16(&dev, legacy+2, (pciread(&dev, legacy) >> 16) | 0x100);
    for(int i = 0; i < 50 && (pciread(&dev, legacy) & 0x10000); i++)
      usbwait(1);
    if(pciread(&dev, legacy) & 0x10000){
      cprintf("usb: ehci BIOS ownership handoff timed out\n");
      return -1;
    }
    pci_write16(&dev, legacy+4, 0);
  }
  r[INTR] = 0;
  r[COMMAND] &= ~RUN;
  for(int i = 0; i < 50 && !(r[STATUS] & HALTED); i++)
    usbwait(1);
  if(!(r[STATUS] & HALTED)){
    cprintf("usb: ehci controller did not halt status 0x%x\n", r[STATUS]);
    return -1;
  }
  r[COMMAND] = RESET;
  if(wait_clear(&r[COMMAND], RESET) < 0){
    cprintf("usb: ehci controller reset timed out\n");
    return -1;
  }
  struct ehci_dma *d = (struct ehci_dma *)kalloc();
  if(!d)
    return -1;
  memset(d, 0, sizeof *d);
  h->dma = d;
  d->qh[0] = V2P(d->qh) | 2;
  d->qh[1] = 1U << 15;
  d->qh[4] = 1;
  d->qh[5] = 1;
  r[SEGMENT] = 0;
  r[ASYNC] = V2P(d->qh);
  r[COMMAND] = RUN;
  r[CONFIG] = 1;
  for(uint i = 0; i < h->ports; i++)
    r[PORT+i] = (r[PORT+i] & ~PORT_CHANGES) | 0x1000;
  usbwait(3);
  return 0;
}

static int
status(struct usbhost *h, uint port)
{
  volatile uint *reg = h->regs + PORT + port;
  uint bits = *reg;
  *reg = (bits & ~PORT_CHANGES) | (bits & 2);
  return bits & 0x2000 ? 0 : bits & 3;
}

static int
reset(struct usbhost *h, uint port)
{
  volatile uint *reg = h->regs + PORT + port;
  uint bits = *reg;
  if(!(bits & 1) || (bits & 0x2000))
    return -1;
  *reg = (bits & ~(PORT_CHANGES | 4)) | 0x100;
  usbwait(6);
  *reg &= ~(PORT_CHANGES | 0x100);
  if(wait_clear(reg, 0x100) < 0)
    return -1;
  usbwait(2);
  if((*reg & 5) == 5)
    return 0;
  if(h->companion)
    *reg = (*reg & ~PORT_CHANGES) | 0x2000;
  return -1;
}

static int
packet(struct usbhost *h, uint addr, uint ep, uint max, uint direction,
       uint toggle, void *data, uint n)
{
  volatile uint *r = h->regs;
  struct ehci_dma *d = h->dma;
  uint pid = direction == USB_SETUP ? 2 : direction == USB_IN ? 1 : 0;

  if(wait_clear(&r[STATUS], ASYNC_STATUS) < 0)
    goto fault;
  if(direction != USB_IN && n)
    memmove(d->data, data, n);
  d->td[0] = d->td[1] = 1;
  d->td[2] = (toggle << 31) | (n << 16) | (3U << 10) | (pid << 8) | 0x80;
  d->td[3] = V2P(d->data);
  // every dma address fits below four gibibytes
  for(uint i = 0; i < NELEM(d->td_high); i++)
    d->td_high[i] = d->qh_high[i] = 0;
  d->qh[1] = addr | (ep << 8) | (2U << 12) | (1U << 14) |
            (1U << 15) | (max << 16);
  d->qh[2] = 1U << 30;
  d->qh[3] = 0;
  d->qh[4] = V2P(d->td);
  d->qh[5] = 1;
  d->qh[6] = 0;
  __sync_synchronize();
  r[COMMAND] |= ASYNC_ENABLE;
  for(int i = 0; i < 50; i++){
    uint done = d->td[2];
    if(!(done & 0x80)){
      r[COMMAND] &= ~ASYNC_ENABLE;
      if(wait_clear(&r[STATUS], ASYNC_STATUS) < 0)
        goto fault;
      __sync_synchronize();
      uint left = (done >> 16) & 0x7fff;
      if((done & 0x7c) || left > n)
        return -1;
      if(direction == USB_IN && n != left)
        memmove(data, d->data, n-left);
      return n-left;
    }
    usbwait(1);
  }
fault:
  // stop issuing new requests without reusing the dma storage
  h->broken = 1;
  r[COMMAND] &= ~(ASYNC_ENABLE | RUN);
  return -1;
}

void
ehcifound(const struct pci_device *d)
{
  uint bar = pciread(d, 0x10);
  if(!bar || (bar & 15))
    return;
  pci_enable(d, 6);
  volatile uint *cap = iomap(bar, 4096);
  if(!cap)
    return;
  uint offset = cap[0] & 255, ports = cap[1] & 15;
  if(offset < 12 || (offset & 3) || !ports || ports > USB_PORT_COUNT)
    return;
  struct usbhost *h = usbhost_register("ehci");
  if(h){
    h->regs = cap + offset/4;
    h->ports = ports;
    h->legacy = (cap[2] >> 8) & 255;
    h->companion = !!(cap[1] & 0xf000);
    h->highspeed = 1;
    h->pci_address = d->address;
    h->start = start;
    h->status = status;
    h->reset = reset;
    h->packet = packet;
  }
}
