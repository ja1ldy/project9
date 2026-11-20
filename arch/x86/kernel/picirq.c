/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "x86.h"
#include "traps.h"
#include "p9abi.h"

#define PIC1 0x20
#define PIC2 0xa0
#define CASCADE 2
#define PIT_DATA 0x40
#define PIT_COMMAND 0x43
#define PIT_HZ 1193182

extern uint ioapicaddr;
static ushort mask = 0xffff;

void
picenable(int irq)
{
  if(irq < 0 || irq >= 16)
    return;
  mask &= ~(1U << irq);
  if(irq >= 8)
    mask &= ~(1U << CASCADE);
  outb(PIC1+1, mask);
  outb(PIC2+1, mask >> 8);
}

void
picinit(void)
{
  uint divisor = PIT_HZ / P9_HZ;

  outb(PIC1+1, 0xff);
  outb(PIC2+1, 0xff);
  if(ioapicaddr)
    return;

  // put the legacy interrupts at the same vectors as the apic interrupts
  outb(PIC1, 0x11);
  outb(PIC2, 0x11);
  outb(PIC1+1, T_IRQ0);
  outb(PIC2+1, T_IRQ0+8);
  outb(PIC1+1, 1 << CASCADE);
  outb(PIC2+1, CASCADE);
  outb(PIC1+1, 1);
  outb(PIC2+1, 1);
  outb(PIT_COMMAND, 0x34);
  outb(PIT_DATA, divisor);
  outb(PIT_DATA, divisor >> 8);
  picenable(IRQ_TIMER);
}

void
irqeoi(int irq)
{
  if(ioapicaddr){
    lapiceoi();
    return;
  }
  if(irq < 0 || irq >= 16)
    return;
  if(irq == 7 || irq == 15){
    int pic = irq == 7 ? PIC1 : PIC2;
    outb(pic, 0x0b);
    if(!(inb(pic) & 0x80)){
      if(irq == 15)
        outb(PIC1, 0x20);
      return;
    }
  }
  if(irq >= 8)
    outb(PIC2, 0x20);
  outb(PIC1, 0x20);
}
