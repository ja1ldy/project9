/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "usbhost.h"

#define SECTOR 512

struct pipe_usb {
  uint endpoint, max, toggle;
};

struct usb_disk {
  struct usbhost *host;
  uint port, address, sectors, tag;
  struct pipe_usb input, output;
  int active;
};

static struct usbhost hosts[USB_HOST_COUNT];
static struct usb_disk disks[USB_DISK_COUNT];
static struct sleeplock lock;
static uint nhosts, ndisks, online;
static struct usb_disk *cached_disk;
static uint cached_sector;
static uchar sector_data[SECTOR];

struct usbhost *
usbhost_register(const char *name)
{
  if(nhosts == NELEM(hosts))
    return 0;
  struct usbhost *h = &hosts[nhosts++];
  h->name = name;
  return h;
}

void
usbwait(uint duration)
{
  acquire(&tickslock);
  uint start = ticks;
  while(ticks-start < duration)
    sleep(&ticks, &tickslock);
  release(&tickslock);
}

static uint
little(const uchar *p, uint bytes)
{
  uint v = 0;
  for(uint i = 0; i < bytes; i++)
    v |= (uint)p[i] << (8*i);
  return v;
}

static uint
big(const uchar *p)
{
  return (uint)p[0]<<24 | (uint)p[1]<<16 | (uint)p[2]<<8 | p[3];
}

static void
put(uchar *p, uint v)
{
  for(uint i = 0; i < 4; i++)
    p[i] = v >> (8*i);
}

static int
control(struct usbhost *h, uint addr, uint max, uchar request_type,
        uchar request, uint value, uint index, uchar *buf, uint length)
{
  uchar setup[8] = {request_type, request, value, value >> 8,
                    index, index >> 8, length, length >> 8};
  uint direction = request_type & 0x80 ? USB_IN : USB_OUT, toggle = 1, done = 0;

  if(h->broken || h->packet(h, addr, 0, max, USB_SETUP, 0, setup, sizeof setup) != sizeof setup)
    return -1;
  while(done < length){
    uint chunk = length-done < max ? length-done : max;
    int count = h->packet(h, addr, 0, max, direction, toggle, buf+done, chunk);
    if(count < 0 || (uint)count > chunk || (direction == USB_OUT && (uint)count != chunk))
      return -1;
    done += count;
    toggle ^= 1;
    if((uint)count < chunk)
      break;
  }
  if(h->packet(h, addr, 0, max, length && direction == USB_IN ? USB_OUT : USB_IN,
               1, 0, 0) != 0)
    return -1;
  return done;
}

static int
bulk(struct usb_disk *d, uint direction, uchar *buf, uint length)
{
  struct pipe_usb *p = direction == USB_IN ? &d->input : &d->output;
  uint done = 0;

  while(done < length){
    uint chunk = length-done < p->max ? length-done : p->max;
    int count = d->host->packet(d->host, d->address, p->endpoint, p->max,
                                direction, p->toggle, buf+done, chunk);
    if(count < 0 || (uint)count > chunk || (direction == USB_OUT && (uint)count != chunk))
      return -1;
    p->toggle ^= 1;
    done += count;
    if((uint)count < chunk)
      break;
  }
  return done;
}

static int
scsi(struct usb_disk *d, const uchar *cmd, uint cmdlen, uchar *data, uint length)
{
  uchar request[31] = {0}, reply[13];
  uint tag = ++d->tag;

  if(d->host->broken)
    return -1;
  put(request, 0x43425355);
  put(request+4, tag);
  put(request+8, length);
  request[12] = length ? 0x80 : 0;
  request[14] = cmdlen;
  memmove(request+15, cmd, cmdlen);
  if(bulk(d, USB_OUT, request, sizeof request) != sizeof request)
    return -1;
  int count = bulk(d, USB_IN, data, length);
  if(count < 0 || bulk(d, USB_IN, reply, sizeof reply) != sizeof reply ||
     little(reply, 4) != 0x53425355 || little(reply+4, 4) != tag ||
     reply[12] > 1 || little(reply+8, 4) != length-count)
    return -1;
  if(reply[12])
    return 1;
  return (uint)count == length ? 0 : -1;
}

static int
capacity(struct usb_disk *d)
{
  uchar ready[6] = {0}, sense[6] = {3,0,0,0,18}, summary[18];
  uchar get[10] = {0x25}, result[8];
  int r = scsi(d, ready, sizeof ready, 0, 0);

  if(r < 0)
    return -1;
  if(r && (scsi(d, sense, sizeof sense, summary, sizeof summary) < 0 ||
           scsi(d, ready, sizeof ready, 0, 0) != 0))
    return -1;
  if(scsi(d, get, sizeof get, result, sizeof result) != 0 ||
     big(result+4) != SECTOR || big(result) >= 0x7fffffffU)
    return -1;
  d->sectors = big(result)+1;
  return 0;
}

static void
enumerate(struct usbhost *h, uint port)
{
  uchar desc[256];
  struct usb_disk candidate = { .host = h, .port = port, .address = port+1 };
  uint configuration, max, total, active = 0;
  const char *phase = "port reset or companion routing";

  if(ndisks == NELEM(disks))
    return;
  if(h->reset(h, port) < 0)
    goto failed;
  phase = "device descriptor";
  if(control(h, 0, 8, 0x80, 6, 0x100, 0, desc, 8) != 8)
    goto failed;
  max = desc[7];
  if(desc[0] != 18 || desc[1] != 1 || (max != 8 && max != 16 && max != 32 && max != 64))
    goto failed;
  phase = "set address";
  if(control(h, 0, max, 0, 5, candidate.address, 0, 0, 0) != 0)
    goto failed;
  usbwait(2);
  phase = "configuration descriptor";
  if(control(h, candidate.address, max, 0x80, 6, 0x200, 0, desc, 9) != 9 ||
     desc[0] != 9 || desc[1] != 2)
    goto failed;
  total = little(desc+2, 2);
  if(total < 9 || total > sizeof desc ||
     control(h, candidate.address, max, 0x80, 6, 0x200, 0, desc, total) != total ||
     little(desc+2, 2) != total)
    goto failed;
  configuration = desc[5];
  for(uint off = 9; off < total; ){
    const uchar *p = desc+off;
    uint n = p[0];
    if(n < 2 || n > total-off)
      goto failed;
    if(p[1] == 4 && n >= 9){
      active = p[3] == 0 && p[5] == 8 && p[6] == 6 && p[7] == 0x50;
      candidate.input.max = candidate.output.max = 0;
    } else if(p[1] == 5 && n >= 7 && active && (p[3] & 3) == 2){
      struct pipe_usb *pipe = p[2] & 128 ? &candidate.input : &candidate.output;
      uint packet = little(p+4, 2);
      if((p[2] & 0x70) || !(p[2] & 15) ||
         (packet != 8 && packet != 16 && packet != 32 && packet != 64 &&
          (packet != 512 || !h->highspeed)) || pipe->max)
        goto failed;
      pipe->endpoint = p[2] & 15;
      pipe->max = packet;
      if(candidate.input.max && candidate.output.max)
        break;
    }
    off += n;
  }
  if(!configuration || !candidate.input.max || !candidate.output.max){
    cprintf("usb: %s port %d has no supported storage interface\n", h->name, port+1);
    return;
  }
  phase = "set configuration";
  if(control(h, candidate.address, max, 0, 9, configuration, 0, 0, 0) != 0)
    goto failed;
  phase = "SCSI readiness or capacity";
  if(capacity(&candidate) < 0)
    goto failed;
  candidate.active = 1;
  disks[ndisks] = candidate;
  cprintf("usb: %s port %d /dev/usb%d read-only %d sectors\n", h->name, port+1,
          ndisks, candidate.sectors);
  ndisks++;
  return;

failed:
  cprintf("usb: %s port %d probe failed at %s\n", h->name, port+1, phase);
}

static void
detach(struct usb_disk *disk)
{
  disk->active = 0;
  if(cached_disk == disk)
    cached_disk = 0;
  cprintf("usb: detached /dev/usb%d\n", (int)(disk-disks));
}

int
usbscan(void)
{
  acquiresleep(&lock);
  for(uint i = 0; i < nhosts; i++){
    struct usbhost *h = &hosts[i];
    if(h->broken)
      continue;
    for(uint p = 0; p < h->ports && !h->broken; p++){
      int state = h->status(h, p), present = !!(state & 1);
      if(h->present[p] == present && !(state & 2))
        continue;
      for(uint j = 0; j < ndisks; j++){
        struct usb_disk *d = &disks[j];
        if(d->active && d->host == h && d->port == p)
          detach(d);
      }
      h->present[p] = present;
      if(present)
        enumerate(h, p);
    }
  }
  releasesleep(&lock);
  return online;
}

int
usbdisks(void)
{
  int mask = 0;

  acquiresleep(&lock);
  for(uint i = 0; i < ndisks; i++)
    if(disks[i].active && !disks[i].host->broken)
      mask |= 1U << i;
  releasesleep(&lock);
  return mask;
}

static int
available(int minor)
{
  return minor >= 0 && minor < USB_DISK_COUNT && (usbdisks() & (1U << minor));
}

static int
usbread(struct inode *ip, char *dst, int n, uint off, int nonblock)
{
  int result = -1;
  uint block = off/SECTOR, offset = off%SECTOR;
  if(myproc()->uid || ip->minor < 0 || ip->minor >= USB_DISK_COUNT)
    return -1;
  acquiresleep(&lock);
  struct usb_disk *d = &disks[ip->minor];
  if(!d->active || d->host->broken)
    goto out;
  if(d->host->status(d->host, d->port) != 1){
    d->host->present[d->port] = 0;
    detach(d);
    goto out;
  }
  if(block >= d->sectors || off >= 0x7fffffffU){
    result = 0;
    goto out;
  }
  if(cached_disk != d || cached_sector != block){
    if(nonblock)
      goto out;
    uchar command[10] = {0x28, 0, block >> 24, block >> 16, block >> 8, block,
                         0, 0, 1, 0};
    cached_disk = 0;
    if(scsi(d, command, sizeof command, sector_data, SECTOR) != 0)
      goto out;
    cached_disk = d;
    cached_sector = block;
  }
  result = (uint)n < SECTOR-offset ? n : SECTOR-offset;
  if((uint)result > 0x7fffffffU-off)
    result = 0x7fffffffU-off;
  memmove(dst, sector_data+offset, result);
out:
  releasesleep(&lock);
  return result;
}

void
usbinit(void)
{
  initsleeplock(&lock, "usb storage");
  devsw[DEV_USB].read = usbread;
  devsw[DEV_USB].available = available;
  for(uint i = 0; i < nhosts; i++){
    struct usbhost *h = &hosts[i];
    if(h->start(h) == 0){
      online++;
      cprintf("usb: %s controller %d root ports\n", h->name, h->ports);
    } else {
      h->broken = 1;
      cprintf("usb: %s startup failed\n", h->name);
    }
  }
  if(online)
    usbscan();
}
