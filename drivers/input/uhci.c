/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "pci.h"
#include "x86.h"

#define CONTROLLERS 8
#define HID_COUNT 4
#define CONTROL_TD 35
#define END 1
#define QH 2
#define ACTIVE (1U << 23)
#define TD_ERROR (0x3bU << 17)
#define LOW (1U << 26)
#define IN 0x69
#define OUT 0xe1
#define SETUP 0x2d

struct td { volatile uint next, status, token, buffer; };
struct queue { volatile uint next, element; uint padding[2]; };
struct hid {
  struct queue queue;
  struct td td;
  uchar buffer[64], previous[8];
  uchar protocol, address, endpoint, packet, low, toggle;
} __attribute__((aligned(16)));
struct memory {
  struct queue control;
  struct td descriptors[CONTROL_TD];
  struct hid hid[HID_COUNT];
  uchar request[8], buffer[256];
};

_Static_assert(sizeof(struct memory) <= 4096, "uhci dma structures must fit one page");
_Static_assert(sizeof(struct hid) % 16 == 0, "check uhci descriptors");

struct controller {
  ushort io;
  struct memory *mem;
  uint *frames;
  uint nextaddr, devices;
  int ready;
};

static struct controller controllers[CONTROLLERS];
static uint ncontrollers;

void
uhcifound(const struct pci_device *device)
{
  uint bar = pciread(device, 0x20);

  if(ncontrollers == CONTROLLERS || !(bar & 1) || bar >= 0xffe0 || (bar & ~31U) == 0)
    return;
  pci_enable(device, 5);
  pci_write16(device, 0xc0, 0x8f00);
  controllers[ncontrollers++].io = bar & ~31U;
}

static void
delay(uint n)
{
  uint start;

  acquire(&tickslock);
  start = ticks;
  while(ticks - start < n)
    sleep(&ticks, &tickslock);
  release(&tickslock);
}

static void
descriptor(struct td *td, uint next, int low, uint pid, uint addr,
           uint endpoint, uint toggle, uint n, void *buffer)
{
  td->next = next;
  td->token = (((n-1) & 0x7ff) << 21) | (toggle << 19) | (endpoint << 15) |
              (addr << 8) | pid;
  td->buffer = n ? V2P(buffer) : 0;
  td->status = ACTIVE | (3U << 27) | 0x7ff | (low ? LOW : 0);
}

static int
control(struct controller *c, int low, uint addr, uint packet, uchar type,
        uchar request, ushort value, ushort index, uint length)
{
  struct memory *m = c->mem;
  struct td *td = m->descriptors;
  uint i, done, n, which, pid = type & 0x80 ? IN : OUT;
  int error = -1;

  if(!packet || length > sizeof(m->buffer) || (length+packet-1)/packet+2 > CONTROL_TD)
    return -1;
  m->request[0] = type;
  m->request[1] = request;
  m->request[2] = value;
  m->request[3] = value >> 8;
  m->request[4] = index;
  m->request[5] = index >> 8;
  m->request[6] = length;
  m->request[7] = length >> 8;
  descriptor(&td[0], V2P(td+1) | 4, low, SETUP, addr, 0, 0, 8, m->request);
  for(done = 0, which = 1; done < length; done += n, which++){
    n = length - done;
    if(n > packet)
      n = packet;
    descriptor(td+which, V2P(td+which+1) | 4, low, pid, addr, 0, which & 1, n,
               m->buffer+done);
  }
  descriptor(td+which, END, low, length && pid == IN ? OUT : IN, addr, 0, 1, 0, 0);
  __sync_synchronize();
  m->control.element = V2P(td);
  for(i = 0; i < 30; i++){
    if(!(td[which].status & ACTIVE) && (td[which].status & TD_ERROR))
      break;
    if(!(td[which].status & ACTIVE)){
      error = 0;
      break;
    }
    delay(1);
  }
  m->control.element = END;
  if(error)
    delay(1);
  return error;
}

// usb boot reports are translated into the same byte stream as ps2 keys
static const ushort scan[128] = {
  [4]=0x1e,[5]=0x30,[6]=0x2e,[7]=0x20,[8]=0x12,[9]=0x21,[10]=0x22,[11]=0x23,
  [12]=0x17,[13]=0x24,[14]=0x25,[15]=0x26,[16]=0x32,[17]=0x31,[18]=0x18,[19]=0x19,
  [20]=0x10,[21]=0x13,[22]=0x1f,[23]=0x14,[24]=0x16,[25]=0x2f,[26]=0x11,[27]=0x2d,
  [28]=0x15,[29]=0x2c,[30]=2,[31]=3,[32]=4,[33]=5,[34]=6,[35]=7,[36]=8,[37]=9,
  [38]=10,[39]=11,[40]=0x1c,[41]=1,[42]=0x0e,[43]=0x0f,[44]=0x39,[45]=0x0c,
  [46]=0x0d,[47]=0x1a,[48]=0x1b,[49]=0x2b,[51]=0x27,[52]=0x28,[53]=0x29,
  [54]=0x33,[55]=0x34,[56]=0x35,[57]=0x3a,[73]=0xe052,[74]=0xe047,[75]=0xe049,
  [76]=0xe053,[77]=0xe04f,[78]=0xe051,[79]=0xe04d,[80]=0xe04b,[81]=0xe050,[82]=0xe048,
};

static int
contains(const uchar *keys, uchar key)
{
  int i;
  for(i = 2; i < 8; i++)
    if(keys[i] == key)
      return 1;
  return 0;
}

static void
keycode(ushort code, int release)
{
  if(!code)
    return;
  if(code >> 8)
    usb_scancode(code >> 8);
  usb_scancode((code & 127) | (release ? 128 : 0));
}

static void
report_keys(struct hid *h)
{
  static const ushort modifiers[8] = {0x1d, 0x2a, 0x38, 0, 0xe01d, 0x36, 0xe038, 0};
  uchar *data = h->buffer, *old = h->previous;
  int i;

  for(i = 2; i < 8; i++)
    if(data[i] == 1)
      return;
  for(i = 0; i < 8; i++)
    if((old[0] ^ data[0]) & (1U << i))
      keycode(modifiers[i], !(data[0] & (1U << i)));
  for(i = 2; i < 8; i++){
    if(old[i] && old[i] < NELEM(scan) && !contains(data, old[i]))
      keycode(scan[old[i]], 1);
    if(data[i] && data[i] < NELEM(scan) && !contains(old, data[i]))
      keycode(scan[data[i]], 0);
  }
  memmove(old, data, 8);
}

static void
arm(struct hid *h)
{
  h->queue.element = END;
  descriptor(&h->td, END, h->low, IN, h->address, h->endpoint, h->toggle, h->packet,
             h->buffer);
  __sync_synchronize();
  h->queue.element = V2P(&h->td);
}

void
uhci_tick(void)
{
  uint i, j, status, length;

  for(i = 0; i < ncontrollers; i++){
    struct controller *c = &controllers[i];

    if(!c->ready)
      continue;
    for(j = 0; j < c->devices; j++){
      struct hid *h = &c->mem->hid[j];

      status = h->td.status;
      if(status & ACTIVE)
        continue;
      if(!(status & TD_ERROR)){
        __sync_synchronize();
        h->toggle ^= 1;
        length = ((status & 0x7ff) + 1) & 0x7ff;
        if(h->protocol == 1 && length >= 8)
          report_keys(h);
        else if(h->protocol == 2 && length >= 3)
          usb_mouse((signed char)h->buffer[1], -(signed char)h->buffer[2], h->buffer[0] & 7);
      }
      arm(h);
    }
  }
}

static void
enumerate(struct controller *c, ushort port)
{
  struct memory *m = c->mem;
  uint i, interval, total, address, packet, configuration, interface = 0, protocol = 0;
  uint endpoint = 0, maximum = 0, low;
  struct hid *h;

  if(!(inw(port) & 1))
    return;
  outw(port, 0x200);
  delay(6);
  outw(port, 0x00a);
  delay(2);
  outw(port, 4);
  delay(2);
  i = inw(port);
  if((i & 5) != 5)
    return;
  low = !!(i & 0x100);
  if(control(c, low, 0, 8, 0x80, 6, 0x100, 0, 8) < 0)
    return;
  packet = m->buffer[7];
  if(packet != 8 && packet != 16 && packet != 32 && packet != 64)
    return;
  address = c->nextaddr++;
  if(control(c, low, 0, packet, 0, 5, address, 0, 0) < 0)
    return;
  delay(2);
  if(control(c, low, address, packet, 0x80, 6, 0x200, 0, 9) < 0)
    return;
  total = m->buffer[2] | (uint)m->buffer[3] << 8;
  if(total < 9 || total > sizeof(m->buffer) ||
     control(c, low, address, packet, 0x80, 6, 0x200, 0, total) < 0)
    return;
  configuration = m->buffer[5];
  for(i = 0; i + 2 <= total; i += interval){
    uchar *d = m->buffer + i;

    interval = d[0];
    if(interval < 2 || interval > total-i)
      return;
    if(d[1] == 4 && interval >= 9){
      protocol = !d[3] && d[5] == 3 && d[6] == 1 && (d[7] == 1 || d[7] == 2) ? d[7] : 0;
      interface = d[2];
    } else if(d[1] == 5 && protocol && interval >= 7 && (d[2] & 0x80) &&
              (d[3] & 3) == 3){
      endpoint = d[2] & 15;
      maximum = d[4] | (uint)d[5] << 8;
      break;
    }
  }
  if(!endpoint || maximum < (protocol == 1 ? 8 : 3) || maximum > 64 ||
     control(c, low, address, packet, 0, 9, configuration, 0, 0) < 0 ||
     control(c, low, address, packet, 0x21, 11, 0, interface, 0) < 0)
    return;
  h = &m->hid[c->devices];
  h->protocol = protocol;
  h->address = address;
  h->endpoint = endpoint;
  h->packet = maximum;
  h->low = low;
  h->queue.next = END;
  arm(h);
  if(c->devices)
    m->hid[c->devices-1].queue.next = V2P(&h->queue) | QH;
  else
    m->control.next = V2P(&h->queue) | QH;
  c->devices++;
  if(protocol == 2)
    usb_mouse_attach();
  cprintf("usb: uhci boot %s\n", protocol == 1 ? "keyboard" : "mouse");
}

void
uhciinit(void)
{
  uint i, j;
  struct controller *c;

  for(i = 0; i < ncontrollers; i++){
    c = &controllers[i];
    outw(c->io, 2);
    for(j = 0; j < 5 && inw(c->io) & 2; j++)
      delay(1);
    if(inw(c->io) & 2)
      continue;
    c->frames = (uint*)kalloc();
    c->mem = (struct memory*)kalloc();
    if(!c->frames || !c->mem){
      if(c->frames)
        kfree((char*)c->frames);
      if(c->mem)
        kfree((char*)c->mem);
      continue;
    }
    memset(c->mem, 0, sizeof(*c->mem));
    c->mem->control.next = c->mem->control.element = END;
    for(j = 0; j < 1024; j++)
      c->frames[j] = V2P(&c->mem->control) | QH;
    outw(c->io+2, 0x3f);
    outw(c->io+4, 0);
    outw(c->io+6, 0);
    outl(c->io+8, V2P(c->frames));
    outb(c->io+12, 64);
    __sync_synchronize();
    outw(c->io, 0x41);
    c->nextaddr = 1;
    for(j = 0; j < 2 && c->devices < HID_COUNT; j++)
      enumerate(c, c->io+0x10+j*2);
    __sync_synchronize();
    c->ready = 1;
  }
}
