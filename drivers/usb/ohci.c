/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "pci.h"
#include "usbhost.h"

enum {
  CONTROL = 1, COMMAND = 2, INTR_STATUS = 3, INTR_DISABLE = 5, HCCA = 6,
  CONTROL_HEAD = 8, FRAME_INTERVAL = 13, PERIODIC_START = 16,
  RH_DESCRIPTOR = 18, RH_STATUS = 20, RH_PORT = 21
};

struct ohci_dma {
  uchar hcca[256];
  volatile uint ed[4], td[4], tail[4];
  uchar data[512];
};

_Static_assert(sizeof(struct ohci_dma) <= 4096, "ohci dma fits a page");

static int
start(struct usbhost *h)
{
  volatile uint *r = h->regs;

  if(r[CONTROL] & 0x100){
    r[COMMAND] = 8;
    for(int i = 0; i < 50 && (r[CONTROL] & 0x100); i++)
      usbwait(1);
    if(r[CONTROL] & 0x100)
      return -1;
  }
  r[INTR_DISABLE] = ~0U;
  r[CONTROL] = 0;
  r[COMMAND] = 1;
  for(int i = 0; i < 10 && (r[COMMAND] & 1); i++)
    usbwait(1);
  if(r[COMMAND] & 1)
    return -1;

  struct ohci_dma *d = (struct ohci_dma *)kalloc();
  if(!d)
    return -1;
  memset(d, 0, sizeof *d);
  h->dma = d;
  d->ed[0] = 1U << 14;
  r[HCCA] = V2P(d->hcca);
  r[CONTROL_HEAD] = V2P(d->ed);
  uint interval = 11999;
  r[FRAME_INTERVAL] = ((r[FRAME_INTERVAL] ^ 0x80000000U) & 0x80000000U) |
                      ((interval-210)*6/7 << 16) | interval;
  r[PERIODIC_START] = interval * 9/10;
  r[CONTROL] = 0x90;
  r[RH_STATUS] = 1U << 16;
  for(uint i = 0; i < h->ports; i++)
    r[RH_PORT+i] = 1U << 8;
  usbwait(((r[RH_DESCRIPTOR] >> 24) + 4)/5 + 2);
  return 0;
}

static int
status(struct usbhost *h, uint port)
{
  volatile uint *reg = h->regs + RH_PORT + port;
  uint bits = *reg;
  *reg = bits & 0x1f0000;
  return (bits & 1) | ((bits >> 15) & 2);
}

static int
reset(struct usbhost *h, uint port)
{
  volatile uint *reg = h->regs + RH_PORT + port;

  if((*reg & 0x201) != 1)
    return -1;
  *reg = 1U << 4;
  for(int i = 0; i < 20 && (*reg & 0x11) == 0x11; i++)
    usbwait(1);
  if((*reg & 0x13) != 3)
    return -1;
  *reg = 0x1f0000;
  usbwait(2);
  return 0;
}

static int
packet(struct usbhost *h, uint addr, uint ep, uint max, uint direction,
       uint toggle, void *data, uint n)
{
  struct ohci_dma *d = h->dma;
  volatile uint *r = h->regs;
  uint physical = V2P(d->data);
  uint pid = direction == USB_SETUP ? 0 : direction == USB_OUT ? 1 : 2;

  if(direction != USB_IN && n)
    memmove(d->data, data, n);
  d->td[0] = (15U << 28) | ((2+toggle) << 24) | (pid << 19) | (1U << 18);
  d->td[1] = n ? physical : 0;
  d->td[2] = V2P(d->tail);
  d->td[3] = n ? physical+n-1 : 0;
  d->ed[1] = V2P(d->tail);
  d->ed[2] = V2P(d->td);
  __sync_synchronize();
  d->ed[0] = addr | (ep << 7) | (max << 16);
  r[COMMAND] = 2;
  for(int i = 0; i < 50; i++){
    uint done = d->ed[2];
    if((done & ~15U) == V2P(d->tail) || (done & 1)){
      __sync_synchronize();
      d->ed[0] |= 1U << 14;
      for(int j = 0; j < 50 && !(r[INTR_STATUS] & 2); j++)
        usbwait(1);
      if(!(r[INTR_STATUS] & 2))
        break;
      r[INTR_STATUS] = 2;
      if((done & 1) || d->td[0] >> 28)
        return -1;
      uint remaining = d->td[1];
      if(remaining && (remaining < physical || remaining > physical+n))
        return -1;
      uint count = remaining ? remaining-physical : n;
      if(direction == USB_IN && count)
        memmove(data, d->data, count);
      return count;
    }
    usbwait(1);
  }
  // keep the dma page reserved if the controller stopped responding
  h->broken = 1;
  r[CONTROL] = 0;
  return -1;
}

void
ohcifound(const struct pci_device *d)
{
  uint bar = pciread(d, 0x10);
  if(!bar || (bar & 15))
    return;
  pci_enable(d, 6);
  volatile uint *r = iomap(bar, 4096);
  if(!r || (r[0] & 0xfe) != 0x10)
    return;
  uint ports = r[RH_DESCRIPTOR] & 255;
  if(!ports || ports > USB_PORT_COUNT)
    return;
  struct usbhost *h = usbhost_register("ohci");
  if(h){
    h->regs = r;
    h->ports = ports;
    h->start = start;
    h->status = status;
    h->reset = reset;
    h->packet = packet;
  }
}
