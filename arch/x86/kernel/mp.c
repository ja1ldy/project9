/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// Multiprocessor support
// Search memory for MP description structures.
// http://developer.intel.com/design/pentium/datashts/24201606.pdf

#include "types.h"
#include "acpi.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mp.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"

struct cpu cpus[NCPU];
int ncpu;
uchar ioapicid;
uint ioapicaddr;
uint irq_gsi[16];
ushort irq_flags[16];
const char *mp_source = "legacy PIC";

static uint
dword(const uchar *p)
{
  return p[0] | (uint)p[1] << 8 | (uint)p[2] << 16 | (uint)p[3] << 24;
}

static int
mmio_page(uint pa)
{
  return pa >= DEVSPACE && !(pa & (PGSIZE-1));
}

static void
bsp_first(void)
{
  uchar bsp = lapicid();
  for(int i = 0; i < ncpu; i++)
    if(cpus[i].apicid == bsp){
      cpus[i].apicid = cpus[0].apicid;
      cpus[0].apicid = bsp;
      return;
    }
  // firmware may list more cpus than the kernel can start
  cpus[0].apicid = bsp;
}

static int
madtinit(void)
{
  const uchar *madt = acpifind("APIC");
  uchar ids[NCPU], seen[256] = {0}, id = 0;
  uint gsi[16], lapicaddr, irqaddr = 0, length, cpus_found = 0;
  ushort flags[16] = {0}, overrides = 0;

  if(!madt || (length = dword(madt+4)) < 44)
    return 0;
  lapicaddr = dword(madt+36);
  for(uint i = 0; i < 16; i++)
    gsi[i] = i;
  for(uint off = 44; off < length; ){
    const uchar *item = madt+off;
    uint n;
    if(length-off < 2 || (n = item[1]) < 2 || n > length-off)
      return 0;
    switch(item[0]){
    case 0:
      if(n < 8)
        return 0;
      if((dword(item+4) & 1) && !seen[item[3]]){
        seen[item[3]] = 1;
        if(cpus_found < NCPU)
          ids[cpus_found++] = item[3];
      }
      break;
    case 1:
      if(n < 12)
        return 0;
      if(!dword(item+8) && !irqaddr && mmio_page(dword(item+4))){
        irqaddr = dword(item+4);
        id = item[2];
      }
      break;
    case 2:
      if(n < 10)
        return 0;
      if(item[2] == 0 && item[3] < 16){
        uint irq = item[3], f = item[8] | (uint)item[9] << 8;
        if((overrides & (1U << irq)) || (f & ~15U) ||
           (f & 3) == 2 || (f & 12) == 8)
          return 0;
        overrides |= 1U << irq;
        gsi[irq] = dword(item+4);
        flags[irq] = f;
      }
      break;
    case 5:
      if(n < 12 || dword(item+8))
        return 0;
      lapicaddr = dword(item+4);
      break;
    }
    off += n;
  }
  if(!cpus_found || !irqaddr || !mmio_page(lapicaddr) || lapicaddr == irqaddr)
    return 0;
  lapic = (volatile uint*)(uptr)MMIOVIRT(lapicaddr);
  if(!seen[lapicid()]){
    lapic = 0;
    return 0;
  }
  ncpu = cpus_found;
  for(int i = 0; i < ncpu; i++)
    cpus[i].apicid = ids[i];
  bsp_first();
  ioapicaddr = irqaddr;
  ioapicid = id;
  memmove(irq_gsi, gsi, sizeof gsi);
  memmove(irq_flags, flags, sizeof flags);
  mp_source = "ACPI MADT";
  return 1;
}

static uchar
sum(uchar *addr, int len)
{
  int i, sum;

  sum = 0;
  for(i=0; i<len; i++)
    sum += addr[i];
  return sum;
}

// Look for an MP structure in the len bytes at addr.
static struct mp*
mpsearch1(uint a, int len)
{
  uchar *e, *p, *addr;

  addr = P2V(a);
  e = addr+len;
  for(p = addr; p < e; p += sizeof(struct mp))
    if(memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0)
      return (struct mp*)p;
  return 0;
}

// Search for the MP Floating Pointer Structure, which according to the
// spec is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) in the last KB of system base memory;
// 3 in the bios rom between 0xe0000 and 0xfffff
static struct mp*
mpsearch(void)
{
  uchar *bda;
  uint p;
  struct mp *mp;

  bda = (uchar *) P2V(0x400);
  if((p = ((bda[0x0F]<<8)| bda[0x0E]) << 4)){
    if((mp = mpsearch1(p, 1024)))
      return mp;
  } else {
    p = ((bda[0x14]<<8)|bda[0x13])*1024;
    if((mp = mpsearch1(p-1024, 1024)))
      return mp;
  }
  return mpsearch1(0xF0000, 0x10000);
}

// Search for an MP configuration table.  For now,
// don't accept the default configurations (physaddr == 0).
// Check for correct signature, calculate the checksum and,
// if correct, check the version.
// To do: check extended table checksum.
static struct mpconf*
mpconfig(struct mp **pmp)
{
  struct mpconf *conf;
  struct mp *mp;

  if((mp = mpsearch()) == 0 || mp->physaddr == 0)
    return 0;
  if((uint)mp->physaddr > PHYSTOP - sizeof(*conf))
    return 0;
  conf = (struct mpconf*) P2V((uint) mp->physaddr);
  if(memcmp(conf, "PCMP", 4) != 0)
    return 0;
  if(conf->version != 1 && conf->version != 4)
    return 0;
  if(conf->length < sizeof(*conf) ||
     conf->length > PHYSTOP - (uint)mp->physaddr ||
     sum((uchar*)conf, conf->length) != 0)
    return 0;
  *pmp = mp;
  return conf;
}

void
mpinit(void)
{
  uchar *p, *e;
  uint size;
  struct mp *mp;
  struct mpconf *conf;
  struct mpproc *proc;
  struct mpioapic *ioapic;

  for(uint i = 0; i < 16; i++)
    irq_gsi[i] = i;
  if(madtinit())
    return;

  conf = mpconfig(&mp);
  if(!conf || !mmio_page((uint)conf->lapicaddr))
    goto legacy;

  lapic = (uint*)(uptr)MMIOVIRT(conf->lapicaddr);
  for(p=(uchar*)(conf+1), e=(uchar*)conf+conf->length; p<e; p+=size){
    size = *p == MPPROC ? sizeof(struct mpproc) : 8;
    if(size > e-p)
      goto legacy;
    switch(*p){
    case MPPROC:
      proc = (struct mpproc*)p;
      if((proc->flags & 1) && ncpu < NCPU)
        cpus[ncpu++].apicid = proc->apicid;
      break;
    case MPIOAPIC:
      ioapic = (struct mpioapic*)p;
      if((ioapic->flags & 1) && (uint)ioapic->addr >= DEVSPACE && !ioapicaddr){
        ioapicaddr = (uint)ioapic->addr;
        ioapicid = ioapic->apicno;
      }
      break;
    case MPBUS: case MPIOINTR: case MPLINTR:
      break;
    default:
      goto legacy;
    }
  }
  if(!ncpu || !ioapicaddr)
    goto legacy;

  bsp_first();
  mp_source = "MP table";
  if(mp->imcrp){
    // bochs doesnt support imcr so this doesnt run on bochs
    // but it would on real hardware
    outb(0x22, 0x70);   // Select IMCR
    outb(0x23, inb(0x23) | 1);  // Mask external interrupts.
  }
  return;

legacy:
  // use the firmware legacy interrupt path when there is no usable mp table
  lapic = 0;
  ioapicaddr = 0;
  ncpu = 1;
  cpus[0].apicid = 0;
}
