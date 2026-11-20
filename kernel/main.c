/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "boothw.h"
#include "install.h"
#include "defs.h"
#include "param.h"
#include "pci.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

uint physlimit = PHYSTOP;
static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
extern const char *mp_source;
extern char end[]; // first address after kernel loaded from ELF file

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  const struct boothw *hw = P2V(BOOTHW_ADDR);

  if(hw->magic == BOOTHW_MAGIC && hw->memend >= 8 * 1024 * 1024 &&
     hw->memend < physlimit)
    physlimit = hw->memend & ~(PGSIZE-1);
  kinit1(end, P2V(4*1024*1024)); // phys page allocator
  fbinit();
  kvmalloc();      // kernel page table
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  seginit();       // segment descriptors
  picinit();       // disable pic
  ioapicinit();    // another interrupt controller
  consoleinit();   // console hardware
  uartinit();      // serial port
#ifdef P9_KERNEL64
  cprintf("Project9 x86_64 kernel\n");
#endif
  cprintf("cpu: %s reports %d processor%s\n", mp_source, ncpu, ncpu == 1 ? "" : "s");
  acpiinit();
  cprintf("cpu: %s floating point context\n", fpumode());
  pinit();         // process table
  ps2init();
  memorydevinit();
  rtcinit();
  fbdevinit();
  tvinit();        // trap vectors
  fileinit();
  accountinit();      // file table
  startothers();   // start other processors
  if(hw->magic == BOOTHW_MAGIC && hw->ram_root_bytes &&
     hw->ram_root_bytes <= INSTALL_RAM_MAX && physlimit > INSTALL_RAM_BASE &&
     hw->ram_root_bytes < physlimit - INSTALL_RAM_BASE){
    kinit2(P2V(4*1024*1024), P2V(INSTALL_RAM_BASE));
    freerange(P2V(INSTALL_RAM_BASE + hw->ram_root_bytes), P2V(physlimit));
  } else
    kinit2(P2V(4*1024*1024), P2V(physlimit));
  pciinit();
  userinit();      // first user process
  mpmain();        // finish this processors setup
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

// Common CPU setup code.
static void
mpmain(void)
{
  cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
  idtinit();       // load idt register
  xchg(&(mycpu()->started), 1); // tell startothers() we're up
  scheduler();     // start running processes
}

#ifndef P9_KERNEL64
extern pde_t entrypgdir[];  // For entry.S
#endif

// Start the non-boot (AP) processors.
static void
startothers(void)
{
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_start.
  code = P2V(0x7000);
  memmove(code, _binary_entryother_start, (uptr)_binary_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == mycpu())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    stack = kalloc();
    if(!stack){
      cprintf("cpu: no stack for apic %d\n", c->apicid);
      break;
    }
#ifdef P9_KERNEL64
    extern uchar bootstrap_pml4[];
    *(uptr*)(code-8) = (uptr)stack + KSTACKSIZE;
    *(uptr*)(code-16) = (uptr)mpenter;
    *(uint*)(code-24) = (uptr)bootstrap_pml4;
#else
    *(void**)(code-4) = stack + KSTACKSIZE;
    *(void(**)(void))(code-8) = mpenter;
    *(int**)(code-12) = (void *) V2P(entrypgdir);

#endif
    lapicstartap(c->apicid, V2P(code));

    // wait for cpu to finish mpmain()
    for(uint tries = 0; tries < 500 && !__atomic_load_n(&c->started, __ATOMIC_ACQUIRE); tries++)
      lapicdelay(1000);
    if(!__atomic_load_n(&c->started, __ATOMIC_ACQUIRE)){
      // leave the trampoline and its stack reserved for a late processor
      cprintf("cpu: startup timed out at apic %d\n", c->apicid);
      break;
    }
  }
}

#ifndef P9_KERNEL64
// the boot page table used in entry s and entryother s
// page directories and page tables must start on page boundaries
// hence the aligned attribute
// pte ps in a page directory entry enables 4mbyte pages

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
  // Map VA's [0, 4MB) to PA's [0, 4MB)
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
  [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

#endif

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

