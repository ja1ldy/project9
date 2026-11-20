/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// The I/O APIC manages hardware interrupts for an SMP system.
// http://www.intel.com/design/chipsets/datashts/29056601.pdf
// See also picirq.c.

#include "types.h"
#include "acpi.h"
#include "defs.h"
#include "traps.h"
#include "param.h"
#include "mmu.h"
#include "memlayout.h"
#include "proc.h"

extern uint ioapicaddr;
static int maxintr;

#define REG_ID     0x00  // Register index: ID
#define REG_VER    0x01  // Register index: version
#define REG_TABLE  0x10  // Redirection table base

// The redirection table starts at REG_TABLE and uses
// two registers to configure each interrupt.
// The first (low) register in a pair contains configuration bits.
// The second (high) register contains a bitmask telling which
// CPUs can serve that interrupt.
#define INT_DISABLED   0x00010000  // Interrupt disabled
#define INT_LEVEL      0x00008000  // Level-triggered (vs edge-)
#define INT_ACTIVELOW  0x00002000  // Active low (vs high)
#define INT_LOGICAL    0x00000800  // Destination is CPU id (vs APIC ID)

volatile struct ioapic *ioapic;

// IO APIC MMIO structure: write reg, then read or write data.
struct ioapic {
  uint reg;
  uint pad[3];
  uint data;
};

static uint
ioapicread(int reg)
{
  ioapic->reg = reg;
  return ioapic->data;
}

static void
ioapicwrite(int reg, uint data)
{
  ioapic->reg = reg;
  ioapic->data = data;
}

void
ioapicinit(void)
{
  int i, id;

  if(!ioapicaddr)
    return;
  ioapic = (volatile struct ioapic*)(uptr)MMIOVIRT(ioapicaddr);
  maxintr = (ioapicread(REG_VER) >> 16) & 0xFF;
  id = ioapicread(REG_ID) >> 24;
  if(id != ioapicid)
    cprintf("ioapicinit: id isn't equal to ioapicid; not a MP\n");

  // Mark all interrupts edge-triggered, active high, disabled,
  // and not routed to any CPUs.
  for(i = 0; i <= maxintr; i++){
    ioapicwrite(REG_TABLE+2*i, INT_DISABLED);
    ioapicwrite(REG_TABLE+2*i+1, 0);
  }
}

void
ioapicenable(int irq, int cpunum)
{
  if(!ioapic){
    picenable(irq);
    return;
  }
  if(irq < 0 || irq >= 16 || irq_gsi[irq] > maxintr ||
     cpunum < 0 || cpunum >= ncpu)
    return;
  uint pin = irq_gsi[irq];
  uint mode = (irq_flags[irq] & 3) == 3 ? INT_ACTIVELOW : 0;
  if((irq_flags[irq] & 12) == 12)
    mode |= INT_LEVEL;
  ioapicwrite(REG_TABLE+2*pin+1, (uint)cpus[cpunum].apicid << 24);
  ioapicwrite(REG_TABLE+2*pin, (T_IRQ0 + irq) | mode);
}
