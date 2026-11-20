/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "acpi.h"
#include "memlayout.h"
#include "pci.h"
#include "x86.h"

static struct {
  ushort a, b, smi, reset;
  uchar enable, reset_value, type_a, type_b;
} sleep5;

static uint
value(const uchar *p, uint n)
{
  uint result = 0, i;
  for(i = 0; i < n; i++)
    result |= (uint)p[i] << (i*8);
  return result;
}

static int
checksum(const uchar *p, uint n)
{
  uchar sum = 0;
  while(n--)
    sum += *p++;
  return sum == 0;
}

static const uchar *
rsdp(uint start, uint end)
{
  const uchar *p;

  for(; start + 20 <= end; start += 16){
    p = P2V(start);
    if(!memcmp(p, "RSD PTR ", 8) && checksum(p, 20))
      return p;
  }
  return 0;
}

extern uint physlimit;

static const uchar *
firmware(uint pa, uint n)
{
  if(!pa || !n || n-1 > ~0U-pa)
    return 0;
  if(pa < physlimit && n <= physlimit-pa)
    return P2V(pa);
  return iomap(pa, n);
}

static const uchar *
table(uint pa)
{
  const uchar *p;
  uint n;

  if(!(p = firmware(pa, 36)))
    return 0;
  n = value(p+4, 4);
  if(n < 36 || n > 0x100000 || !(p = firmware(pa, n)) || !checksum(p, n))
    return 0;
  return p;
}

static int
integer(const uchar **p, const uchar *end)
{
  int byte;

  if(*p == end)
    return -1;
  byte = *(*p)++;
  if(byte <= 1)
    return byte;
  if(byte == 0x0a && *p < end)
    return *(*p)++;
  return -1;
}

static int
types(const uchar *dsdt)
{
  const uchar *p, *q, *end, *last;
  uint extra, size, i;
  int a, b;

  if(!dsdt || memcmp(dsdt, "DSDT", 4))
    return 0;
  end = dsdt + value(dsdt+4, 4);
  for(p = dsdt+36; end-p >= 10; p++){
    if(*p != 8)
      continue;
    q = p+1;
    if(*q == '\\')
      q++;
    if(memcmp(q, "_S5_", 4) || q[4] != 0x12)
      continue;
    q += 5;
    extra = *q >> 6;
    if((uint)(end-q) <= extra+1)
      continue;
    size = *q & (extra ? 15 : 63);
    for(i = 0; i < extra; i++)
      size |= (uint)q[i+1] << (4 + i*8);
    if(size <= extra+1 || size > end-q)
      continue;
    last = q + size;
    q += extra+1;
    if(q == last || *q++ < 2)
      continue;
    a = integer(&q, last);
    b = integer(&q, last);
    if(a >= 0 && a <= 7 && b >= 0 && b <= 7){
      sleep5.type_a = a;
      sleep5.type_b = b;
      return 1;
    }
  }
  return 0;
}

const uchar *
acpifind(const char *signature)
{
  uint base = (uint)*(ushort*)P2V(0x40e) << 4;
  const uchar *id = 0, *root = 0, *entry;
  uint n, step = 4;

  if(base >= 0x80000 && base <= 0x9fc00)
    id = rsdp(base, base+1024);
  if(!id)
    id = rsdp(0xe0000, 0x100000);
  if(!id)
    return 0;
  if(id[15] >= 2 && (uptr)id <= (uptr)P2V(0x100000)-36 &&
     value(id+20, 4) == 36 && checksum(id, 36) && !value(id+28, 4)){
    root = table(value(id+24, 4));
    if(root && !memcmp(root, "XSDT", 4))
      step = 8;
    else
      root = 0;
  }
  if(!root){
    root = table(value(id+16, 4));
    if(!root || memcmp(root, "RSDT", 4))
      return 0;
  }
  n = value(root+4, 4);
  if((n-36) % step || (n-36)/step > 256)
    return 0;
  for(uint i = 36; i+step <= n; i += step){
    if(step == 8 && value(root+i+4, 4))
      continue;
    entry = firmware(value(root+i, 4), 36);
    if(entry && !memcmp(entry, signature, 4)){
      entry = table(value(root+i, 4));
      if(entry)
        return entry;
    }
  }
  return 0;
}

void
acpiinit(void)
{
  const uchar *fadt = acpifind("FACP");
  uint n, flags, a, b, smi, dsdt, reset;

  if(!fadt || (n = value(fadt+4, 4)) < 116)
    return;
  flags = value(fadt+112, 4);
  if((flags & (1U<<10)) && n >= 129 && fadt[116] == 1 && fadt[117] == 8 &&
     !fadt[118] && fadt[119] <= 1 && !value(fadt+124, 4)){
    reset = value(fadt+120, 4);
    if(reset && reset <= 0xffff){
      sleep5.reset = reset;
      sleep5.reset_value = fadt[128];
    }
  }
  if(flags & (1U<<20) || fadt[89] < 2)
    return;
  a = value(fadt+64, 4);
  b = value(fadt+68, 4);
  smi = value(fadt+48, 4);
  dsdt = value(fadt+40, 4);
  if(n >= 148 && !value(fadt+144, 4) && value(fadt+140, 4))
    dsdt = value(fadt+140, 4);
  if(a && a < 0xffff && b < 0xffff && smi <= 0xffff && types(table(dsdt))){
    sleep5.a = a;
    sleep5.b = b;
    sleep5.smi = smi;
    sleep5.enable = fadt[52];
    cprintf("acpi: s5 poweroff available\n");
  }
}

void
acpireset(void)
{
  if(sleep5.reset)
    outb(sleep5.reset, sleep5.reset_value);
}

void
acpipoweroff(void)
{
  ushort a, b = 0;
  int i;

  if(!sleep5.a)
    return;
  if(!(inw(sleep5.a) & 1) && sleep5.smi && sleep5.enable){
    outb(sleep5.smi, sleep5.enable);
    for(i = 0; i < 1000000 && !(inw(sleep5.a) & 1); i++)
      ;
  }
  if(!(inw(sleep5.a) & 1))
    return;
  a = (inw(sleep5.a) & ~0x3c00) | ((uint)sleep5.type_a<<10);
  outw(sleep5.a, a);
  if(sleep5.b){
    b = (inw(sleep5.b) & ~0x3c00) | ((uint)sleep5.type_b<<10);
    outw(sleep5.b, b);
    outw(sleep5.b, b | 0x2000);
  }
  outw(sleep5.a, a | 0x2000);
}
