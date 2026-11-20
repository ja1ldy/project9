/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "p9abi.h"
#include "elf.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
#ifdef P9_KERNEL64
  c->gdt[SEG_KCODE].db = 0;
  c->gdt[SEG_KCODE].rsv1 = 1;
#endif
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
#ifdef P9_KERNEL64
  c->gdt[SEG_UCODE64] = c->gdt[SEG_UCODE];
  c->gdt[SEG_UCODE64].db = 0;
  c->gdt[SEG_UCODE64].rsv1 = 1;
#endif
  lgdt(c->gdt, sizeof(c->gdt));
#ifdef P9_KERNEL64
  _Static_assert(sizeof(struct taskstate) == 104, "amd64 tss size");
  _Static_assert(sizeof(struct gatedesc) == 16, "amd64 idt gate size");
  uptr base = (uptr)&c->ts;
  c->gdt[SEG_TSS] = SEG16(STS_T32A, base, sizeof(c->ts)-1, 0);
  c->gdt[SEG_TSS].s = 0;
  c->gdt[SEG_TSS].db = 0;
  uptr high = base >> 32;
  memmove(&c->gdt[SEG_TSS + 1], &high, sizeof high);
  c->ts.iomb = sizeof(c->ts);
  ltr(SEG_TSS << 3);
#endif
  fpuinit();
}

#ifdef P9_KERNEL64
#define TABLE_BITS 9
#define TABLE_COUNT (1U << TABLE_BITS)
#define TABLE_SPAN (PGSIZE * TABLE_COUNT)
#define KERNEL_PML4 511

static pte_t *
walklevel(pde_t *pgdir, uptr va, int alloc, int target)
{
  pde_t *entry;

  for(int level = 4; level > target; level--){
    entry = &pgdir[(va >> (PTXSHIFT + TABLE_BITS * (level - 1))) & (TABLE_COUNT - 1)];
    if(!(*entry & PTE_P)){
      pde_t *next;
      if(!alloc || !(next = (pde_t*)kalloc()))
        return 0;
      memset(next, 0, PGSIZE);
      *entry = V2P(next) | PTE_P | PTE_W | (va < KERNBASE ? PTE_U : 0);
    }
    pgdir = P2V(PTE_ADDR(*entry));
  }
  return &pgdir[(va >> (PTXSHIFT + TABLE_BITS * (target - 1))) & (TABLE_COUNT - 1)];
}

static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  return walklevel(pgdir, (uptr)va, alloc, 1);
}

// the upper tree belongs to the kernel and may be shared by many processes
static void
free_tables(pde_t *table, int level, int limit)
{
  if(level > 1)
    for(int i = 0; i < limit; i++)
      if(table[i] & PTE_P)
        free_tables(P2V(PTE_ADDR(table[i])), level - 1, TABLE_COUNT);
  kfree((char*)table);
}

#else
// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}
#endif

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  if(!size || size-1 > ~(uptr)0-(uptr)va || (pa & (PGSIZE-1)) ||
     ((perm & PTE_U) && ((uptr)va >= KERNBASE || size > KERNBASE-(uptr)va)))
    return -1;
  a = (char*)PGROUNDDOWN((uptr)va);
  last = (char*)PGROUNDDOWN(((uptr)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
};

// boot discovers a few mmio ranges which every page table needs
#define IO_VIRT MMIOVIRT(0xfc000000U)
static struct kmap io[64];
static uint iomaps;
static uptr iobase = IO_VIRT;

void *
iomap(uint pa, uint bytes)
{
  uint off = pa & (PGSIZE-1);
  uint start = pa - off, size;
  struct kmap *k;

  if(!bytes || bytes > 0x100000 || bytes > ~0U - pa)
    return 0;
  size = PGROUNDUP(off + bytes);
  if(size > ~0U - start)
    return 0;
  for(k = io; k < io + iomaps; k++)
    if(pa >= k->phys_start && pa + bytes <= k->phys_end)
      return (char*)k->virt + pa - k->phys_start;
  if(iomaps == NELEM(io) || size > FB_VIRT - iobase ||
     mappages(kpgdir, (void*)iobase, size, start, PTE_W|PTE_PWT|PTE_PCD) < 0)
    return 0;
  io[iomaps++] = (struct kmap){ (void*)iobase, start, start+size, PTE_W|PTE_PWT|PTE_PCD };
  iobase += size;
  return (void*)(iobase - size + off);
}

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;
  struct kmap kmap[] = {
   { (void*)KERNVIRT, 0,             EXTMEM,    PTE_W}, // I/O space
   { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
   { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
   { (void*)MMIOVIRT(DEVSPACE), DEVSPACE, 0,    PTE_W|PTE_PCD|PTE_PWT}, // more devices
  };

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
#ifdef P9_KERNEL64
  if(kpgdir){
    pgdir[KERNEL_PML4] = kpgdir[KERNEL_PML4];
    return pgdir;
  }
#endif
  if (P2V(PHYSTOP) > (void*)MMIOVIRT(DEVSPACE))
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  for(k = io; k < io + iomaps; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start, k->phys_start, k->perm) < 0){
      freevm(pgdir);
      return 0;
    }
  if(fb_map(pgdir) < 0){
    freevm(pgdir);
    return 0;
  }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  if(!kpgdir)
    panic("cannot map kernel");
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
#ifdef P9_KERNEL64
  mycpu()->ts.rsp[0] = (uptr)p->kstack + KSTACKSIZE;
#else
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting iopl 0 in eflags and iomb beyond the tss segment limit
  // forbids i o instructions e g inb and outb from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
#endif
  lcr3(V2P(p->pgdir));  // switch to processs address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uptr)addr % PGSIZE || (uptr)addr >= KERNBASE ||
     sz > KERNBASE-(uptr)addr || offset > ip->size || sz > ip->size-offset)
    return -1;
  for(i = 0; i < sz; i += PGSIZE){
    pte = walkpgdir(pgdir, addr+i, 0);
    if(!pte || (*pte & (PTE_P | PTE_U | PTE_DEVICE)) != (PTE_P | PTE_U))
      return -1;
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz > FB_USER)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)(uptr)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa, entry;
#ifndef P9_KERNEL64
  uint i;
#endif

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)(uptr)a, 0);
    if(!pte)
#ifdef P9_KERNEL64
      a = (a & ~(TABLE_SPAN - 1)) + TABLE_SPAN - PGSIZE;
#else
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
#endif
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      if(!(*pte & PTE_DEVICE))
        kfree(v);
      *pte = 0;
    }
  }
  // release page table pages that lost their last mapping
#ifdef P9_KERNEL64
  for(a = newsz & ~(TABLE_SPAN - 1); a < oldsz; a += TABLE_SPAN){
    pde_t *dir = walklevel(pgdir, a, 0, 2);
    if(!dir || !(*dir & PTE_P))
      continue;
    pte = P2V(PTE_ADDR(*dir));
    for(entry = 0; entry < TABLE_COUNT; entry++)
      if(pte[entry] & PTE_P)
        break;
    if(entry == TABLE_COUNT){
      *dir = 0;
      kfree((char*)pte);
    }
  }
#else
  for(i = PDX(PGROUNDUP(newsz)); i <= PDX(oldsz-1) && i < PDX(KERNBASE); i++){
    if(!(pgdir[i] & PTE_P))
      continue;
    pte = (pte_t*)P2V(PTE_ADDR(pgdir[i]));
    for(entry = 0; entry < NPTENTRIES; entry++)
      if(pte[entry] & PTE_P)
        break;
    if(entry == NPTENTRIES){
      pgdir[i] = 0;
      kfree((char*)pte);
    }
  }
#endif
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
#ifdef P9_KERNEL64
  free_tables(pgdir, 4, kpgdir ? KERNEL_PML4 : TABLE_COUNT);
#else
  for(uint i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
#endif
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// the loader calls this after copying the segment into physical memory
void
userreadonly(pde_t *pgdir, uint start, uint end)
{
  uint va;

  for(va = start; va < end; va += PGSIZE){
    pte_t *pte = walkpgdir(pgdir, (char*)(uptr)va, 0);
    if(pte)
      *pte &= ~PTE_W;
  }
}

// Given a parent process's page table, create a copy
// of it for a child.
static int
copyrange(pde_t *to, pde_t *from, uint start, uint end)
{
  pte_t *pte;
  uint va;
  char *mem;

  for(va = start; va < end; va += PGSIZE){
    pte = walkpgdir(from, (void*)(uptr)va, 0);
    if(!pte || !(*pte & PTE_P) || (*pte & PTE_DEVICE))
      return -1;
    if((mem = kalloc()) == 0)
      return -1;
    memmove(mem, P2V(PTE_ADDR(*pte)), PGSIZE);
    if(mappages(to, (void*)(uptr)va, PGSIZE, V2P(mem), PTE_FLAGS(*pte)) < 0){
      kfree(mem);
      return -1;
    }
  }
  return 0;
}

pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *to = setupkvm();

  if(to && copyrange(to, pgdir, 0, sz) < 0){
    freevm(to);
    return 0;
  }
  return to;
}

uint
usermapend(struct proc *p, uint addr)
{
  struct filemap *m;

  if(addr < p->sz)
    return p->sz;
  if(addr < FILEMAP_BASE || addr >= FB_USER)
    return 0;
  for(m = p->map; m < p->map + FILEMAP_COUNT; m++)
    if(m->length && addr >= m->start && addr - m->start < m->length)
      return m->start + m->length;
  return 0;
}

int
private_map(struct proc *p, struct inode *inode, uint offset, uint length,
            uint bytes, int writable)
{
  struct filemap *m, *slot = 0;
  uint low = FILEMAP_BASE, rounded, hi, next;

  if(!length || length > FILEMAP_LIMIT || bytes > length ||
     (bytes && (!inode || offset > inode->size || bytes > inode->size-offset)))
    return -P9_EINVAL;
  rounded = PGROUNDUP(length);
  for(m = p->map; m < p->map + FILEMAP_COUNT; m++)
    if(!m->length){
      slot = m;
      break;
    }
  if(!slot)
    return -P9_ENOMEM;
  for(;;){
    if(rounded > FB_USER - low)
      return -P9_ENOMEM;
    hi = low + rounded;
    next = low;
    for(m = p->map; m < p->map + FILEMAP_COUNT; m++)
      if(m->length && m->start < hi && low < m->start + PGROUNDUP(m->length)){
        next = m->start + PGROUNDUP(m->length);
        break;
      }
    if(next == low)
      break;
    low = next;
  }
  if(!allocuvm(p->pgdir, low, hi))
    return -P9_ENOMEM;
  if(bytes && loaduvm(p->pgdir, (char*)(uptr)low, inode, offset, bytes) < 0){
    deallocuvm(p->pgdir, hi, low);
    return -P9_EIO;
  }
  if(!writable)
    userreadonly(p->pgdir, low, hi);
  *slot = (struct filemap){ low, length };
  switchuvm(p);
  return low;
}


int
private_unmap(struct proc *p, uint addr, uint length)
{
  if(addr < FILEMAP_BASE || addr >= FB_USER || addr % PGSIZE ||
     !length || length > FB_USER - addr)
    return -P9_EINVAL;
  uint end = addr + PGROUNDUP(length);
  struct filemap *spare = 0, *map = 0;
  for(struct filemap *m = p->map; m < p->map + FILEMAP_COUNT; m++){
    if(!m->length)
      spare = m;
    if(m->length && m->start <= addr && end <= m->start + PGROUNDUP(m->length))
      map = m;
  }
  if(!map)
    return -P9_EINVAL;
  uint start = map->start, last = map->start + map->length;
  if(addr > start && end < last){
    if(!spare)
      return -P9_ENOMEM;
    *spare = (struct filemap){ end, last-end };
  }
  deallocuvm(p->pgdir, end, addr);
  *map = addr > start ? (struct filemap){ start, addr-start } :
         end < last ? (struct filemap){ end, last-end } : (struct filemap){0};
  switchuvm(p);
  return 0;
}

int
private_clone(struct proc *to, struct proc *from)
{
  uint i;

  for(i = 0; i < FILEMAP_COUNT; i++){
    struct filemap m = from->map[i];

    if(m.length && copyrange(to->pgdir, from->pgdir, m.start, m.start + m.length) < 0)
      return -1;
    to->map[i] = m;
  }
  return 0;
}

void
private_reset(struct proc *p)
{
  memset(p->map, 0, sizeof(p->map));
}

int
userpage(pde_t *pgdir, uint addr, int write)
{
  unsigned int need = PTE_P | PTE_U | (write ? PTE_W : 0);
  pte_t *pte;

  if(addr >= KERNBASE
#ifndef P9_KERNEL64
     || (pgdir[PDX(addr)] & need) != need
#endif
    )
    return 0;
  pte = walkpgdir(pgdir, (void*)(uptr)addr, 0);
  return pte && (*pte & (need | PTE_DEVICE)) == need;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(!pte || (*pte & PTE_P) == 0 || (*pte & PTE_DEVICE))
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  if(!len)
    return 0;
  if(va >= KERNBASE || len > KERNBASE-va)
    return -1;
  for(uint page = PGROUNDDOWN(va); page < va+len; page += PGSIZE)
    if(!userpage(pgdir, page, 1))
      return -1;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)(uptr)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.


void
fbunmapuser(struct proc *p)
{
  if(!p->fbmaplen)
    return;
  deallocuvm(p->pgdir, FB_USER + p->fbmaplen, FB_USER);
  if(p == myproc())
    switchuvm(p);
  if(p->fbmapwrite)
    fbgraphics(-1);
  p->fbmaplen = 0;
  p->fbmapwrite = 0;
}
