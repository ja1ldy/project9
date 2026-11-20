/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "x86.h"
#include "traps.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "p9abi.h"

#define DATA 0x60
#define STATUS 0x64
#define OUT_FULL 0x01
#define IN_FULL 0x02
#define AUX_BYTE 0x20
#define BAD_BYTE 0xc0
#define IRQ_AUX_BIT 0x02
#define INHIBIT_AUX 0x20
#define WAIT_LIMIT 100000
#define QUEUE_SIZE 64

struct queue {
  uint r, w;
  struct input_event data[QUEUE_SIZE];
};

static struct spinlock inputlock;
static struct queue keyboard, pointer, touch;
static int mouse, touchpad, usb_pointer;

static void
enqueue(struct queue *q, struct input_event *event)
{
  acquire(&inputlock);
  if(q->w - q->r == QUEUE_SIZE)
    q->r++;
  q->data[q->w++ % QUEUE_SIZE] = *event;
  wakeup(q);
  selectnotify();
  release(&inputlock);
}

void
ps2key(uint code)
{
  struct input_event event = { .type = INPUT_KEY, .code = code };
  enqueue(&keyboard, &event);
}

void
ps2aux(int byte)
{
  static int used;
  static uchar packet[3];
  struct input_event event = { .type = INPUT_RELATIVE };

  if(byte < 0){
    used = 0;
    return;
  }
  if(!mouse || (!used && !(byte & 8)))
    return;
  packet[used++] = byte;
  if(used < sizeof(packet))
    return;
  used = 0;

  // report the buttons even when the movement counter overflowed
  if(!(packet[0] & 0xc0)){
    event.x = packet[1] - ((packet[0] & 0x10) ? 256 : 0);
    event.y = ((packet[0] & 0x20) ? 256 : 0) - packet[2];
  }
  event.buttons = packet[0] & 7;
  enqueue(&pointer, &event);
  if(touchpad)
    enqueue(&touch, &event);
}

static int
put(uint port, uint value)
{
  uint i;

  for(i = 0; i < WAIT_LIMIT; i++)
    if(!(inb(STATUS) & IN_FULL)){
      outb(port, value);
      return 0;
    }
  return -1;
}

static int
receive(int want_aux)
{
  uint i, status, byte;

  for(i = 0; i < WAIT_LIMIT; i++){
    status = inb(STATUS);
    if(!(status & OUT_FULL))
      continue;
    byte = inb(DATA);
    if(!(status & BAD_BYTE) && !!(status & AUX_BYTE) == want_aux)
      return byte;
  }
  return -1;
}

static int
command(uint cmd)
{
  int i, reply;

  for(i = 0; i < 3; i++){
    if(put(STATUS, 0xd4) < 0 || put(DATA, cmd) < 0)
      return -1;
    reply = receive(1);
    if(reply == 0xfa)
      return 0;
    if(reply != 0xfe)
      break;
  }
  return -1;
}

static int
sliced(uint value)
{
  int shift;

  for(shift = 6; shift >= 0; shift -= 2)
    if(command(0xe8) < 0 || command((value >> shift) & 3) < 0)
      return -1;
  return 0;
}

static int
synaptics(void)
{
  int first, id, last;

  if(sliced(0) < 0 || command(0xe9) < 0)
    return 0;
  first = receive(1);
  id = receive(1);
  last = receive(1);
  return first >= 0 && id == 0x47 && last >= 0;
}

void
usb_mouse_attach(void)
{
  usb_pointer = 1;
}

void
usb_mouse(int x, int y, uint buttons)
{
  struct input_event event = { .type = INPUT_RELATIVE, .x = x, .y = y, .buttons = buttons };
  enqueue(&pointer, &event);
}

static int
point_available(int minor)
{
  return minor == POINTER_MOUSE ? (mouse || usb_pointer) :
         minor == POINTER_TOUCHPAD ? touchpad : 0;
}

static int
read_queue(struct inode *ip, struct queue *q, char *dst, uint n, int nonblock)
{
  uint used = 0;

  if(!n)
    return 0;
  if(n < sizeof(struct input_event))
    return -1;
  iunlock(ip);
  acquire(&inputlock);
  while(q->w == q->r){
    if(nonblock || sleepintr(q, &inputlock) < 0){
      release(&inputlock);
      ilock(ip);
      return nonblock ? -P9_EAGAIN : -P9_EINTR;
    }
  }
  while(n - used >= sizeof(struct input_event) && q->w != q->r){
    memmove(dst + used, &q->data[q->r++ % QUEUE_SIZE], sizeof(struct input_event));
    used += sizeof(struct input_event);
  }
  release(&inputlock);
  ilock(ip);
  return used;
}

static int
keyboard_read(struct inode *ip, char *dst, int n, uint off, int nonblock)
{
  return ip->minor == 0 ? read_queue(ip, &keyboard, dst, n, nonblock) : -1;
}

static int
pointer_read(struct inode *ip, char *dst, int n, uint off, int nonblock)
{
  if(!point_available(ip->minor))
    return -1;
  return read_queue(ip, ip->minor == POINTER_TOUCHPAD ? &touch : &pointer, dst, n, nonblock);
}

static int
inputready(struct inode *ip, int writing)
{
  struct queue *q;
  int ready;

  if(writing)
    return 0;
  q = ip->major == DEV_KEYBOARD ? &keyboard :
      ip->minor == POINTER_TOUCHPAD ? &touch : &pointer;
  acquire(&inputlock);
  ready = q->r != q->w;
  release(&inputlock);
  return ready;
}

void
ps2init(void)
{
  int cfg, i;

  initlock(&inputlock, "ps2");
  devsw[DEV_KEYBOARD].read = keyboard_read;
  devsw[DEV_KEYBOARD].ready = devsw[DEV_POINTER].ready = inputready;
  devsw[DEV_POINTER].read = pointer_read;
  devsw[DEV_POINTER].available = point_available;

  // leave the firmware keyboard setup in place when aux probing fails
  if(put(STATUS, 0xad) < 0)
    return;
  for(i = 0; i < QUEUE_SIZE && (inb(STATUS) & OUT_FULL); i++)
    inb(DATA);
  if(put(STATUS, 0x20) == 0 && (cfg = receive(0)) >= 0 &&
     put(STATUS, 0xa8) == 0){
    if(command(0xf5) == 0 && command(0xf6) == 0){
      touchpad = synaptics();
      // select the relative packet format on a detected synaptics device
      if(touchpad && (sliced(0) < 0 || command(0xf3) < 0 || command(0x14) < 0))
        touchpad = command(0xf6) == 0 ? 0 : -1;
      mouse = touchpad >= 0 && command(0xf4) == 0;
      touchpad = mouse && touchpad == 1;
    }
    if(mouse){
      if(put(STATUS, 0x60) == 0 && put(DATA, (cfg & ~INHIBIT_AUX) | IRQ_AUX_BIT) == 0)
        ioapicenable(IRQ_AUX, 0);
      else
        mouse = touchpad = 0;
    }
    if(!mouse)
      put(STATUS, 0xa7);
  }
  put(STATUS, 0xae);
}
