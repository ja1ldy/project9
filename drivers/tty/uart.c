/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// Intel 8250 serial port (UART).

#include "types.h"
#include "version.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

#define COM1    0x3f8

static int uart;    // set when com1 responds

void
uartinit(void)
{
  char *p;

  // Turn off the FIFO
  outb(COM1+2, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outb(COM1+3, 0x80);    // Unlock divisor
  outb(COM1+0, 115200/9600);
  outb(COM1+1, 0);
  outb(COM1+3, 0x03);    // Lock divisor, 8 data bits.
  outb(COM1+4, 0);
  outb(COM1+1, 0x01);    // Enable receive interrupts.

  // an absent serial port returns all ones
  if(inb(COM1+5) == 0xFF)
    return;
  uart = 1;

  // clear any pending interrupts before enabling the serial irq
  inb(COM1+2);
  inb(COM1+0);
  ioapicenable(IRQ_COM1, 0);

  // send the boot banner to the serial console
  for(p=PROJECT9_RELEASE "\n"; *p; p++)
    uartputc(*p);
}

void
uartputc(int c)
{
  int i;

  if(!uart)
    return;
  for(i = 0; i < 128 && !(inb(COM1+5) & 0x20); i++)
    microdelay(10);
  outb(COM1+0, c);
}

static int
uartgetc(void)
{
  if(!uart)
    return -1;
  if(!(inb(COM1+5) & 0x01))
    return -1;
  return inb(COM1+0);
}

void
uartintr(void)
{
  consoleintr(uartgetc);
}
