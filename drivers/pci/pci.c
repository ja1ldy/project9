/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "pci.h"
#include "usbhost.h"
#include "x86.h"

#define ADDRESS 0xcf8
#define DATA 0xcfc

static int emulator_vga;

uint
pciread(const struct pci_device *d, uint reg)
{
  outl(ADDRESS, d->address | (reg & 0xfc));
  return inl(DATA);
}

void
pci_write16(const struct pci_device *d, uint reg, ushort value)
{
  outl(ADDRESS, d->address | (reg & 0xfc));
  outw(DATA + (reg & 2), value);
}

void
pci_enable(const struct pci_device *d, ushort bits)
{
  pci_write16(d, 4, pciread(d, 4) | bits);
}

void
pciinit(void)
{
  struct pci_device d;
  uint bus, slot, fn, functions;

  for(bus = 0; bus < 256; bus++)
    for(slot = 0; slot < 32; slot++){
      functions = 1;
      for(fn = 0; fn < functions; fn++){
        d.address = 0x80000000U | bus << 16 | slot << 11 | fn << 8;
        d.id = pciread(&d, 0);
        if((d.id & 0xffff) == 0xffff)
          continue;
        if(!fn && (pciread(&d, 12) & 0x00800000))
          functions = 8;
        d.class = pciread(&d, 8) >> 8;
        cprintf("pci: %x:%x.%x id %x class %x\n", bus, slot, fn, d.id, d.class);
        if(d.id == 0x11111234)
          emulator_vga = 1;
        if(d.class == 0x010601)
          ahcifound(&d);
        if(d.class == 0x0c0300)
          uhcifound(&d);
        if(d.class == 0x0c0310)
          ohcifound(&d);
        if(d.class == 0x0c0320)
          ehcifound(&d);
      }
    }
}

int
pci_present(uint id)
{
  return id == 0x11111234 && emulator_vga;
}
