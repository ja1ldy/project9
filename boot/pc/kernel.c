/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "loader.h"
#include "elf.h"
#include "layout.h"
#include "memlayout.h"
#include "boothw.h"
#include "install.h"

#define PACKET_ADDR 0x8000
#define BOUNCE_ADDR 0x8200
#define BOUNCE_BLOCKS 7
#define MAX_SEGMENTS 16

struct packet {
  uchar size, reserved;
  ushort sectors, offset, segment;
  uint lba, high;
};

struct kernel_info {
  uint magic, bytes;
  uint offset64, size64, size32;
};

typedef unsigned long long quad;
struct elf64 {
  uint magic;
  uchar ident[12];
  ushort type, machine;
  uint version;
  quad entry, phoff, shoff;
  uint flags;
  ushort ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};

struct segment64 {
  uint type, flags;
  quad offset, vaddr, paddr, filesz, memsz, align;
};

_Static_assert(sizeof(struct packet) == 16, "bad bios packet layout");
_Static_assert(sizeof(struct elf64) == 64 && sizeof(struct segment64) == 56,
               "bad elf64 header layout");

static int
read_blocks(uint disk, uint lba, uint blocks)
{
  struct packet *packet = physical(PACKET_ADDR);
  int tries;

  for(tries = 0; tries < 3; tries++){
    packet->size = sizeof(*packet);
    packet->reserved = 0;
    packet->sectors = blocks;
    packet->offset = 0;
    packet->segment = BOUNCE_ADDR >> 4;
    packet->lba = lba;
    packet->high = 0;
    bios(0x13, 0x4200, 0, disk);
    __v86.esi = PACKET_ADDR;
    __v86int();
    if(!(__v86.efl & BIOS_CARRY))
      return 0;
    bios(0x13, 0, 0, disk);
    __v86int();
  }
  return -1;
}

static int
read_bytes(uint disk, void *where, uint offset, uint count)
{
  const uchar *buffer = physical(BOUNCE_ADDR);
  uchar *out = where;
  uint skip, n, sectors, i;

  // work in a low memory buffer so the bios can reach it
  while(count){
    skip = offset % BOOT_SECTOR_SIZE;
    n = BOUNCE_BLOCKS * BOOT_SECTOR_SIZE - skip;
    if(n > count)
      n = count;
    sectors = (skip + n + BOOT_SECTOR_SIZE - 1) / BOOT_SECTOR_SIZE;
    if(read_blocks(disk, BOOT_KERNEL_LBA + offset / BOOT_SECTOR_SIZE, sectors) < 0)
      return -1;
    for(i = 0; i < n; i++)
      *out++ = buffer[skip+i];
    offset += n;
    count -= n;
  }
  return 0;
}

static int
in_file(uint offset, uint count, uint limit)
{
  return offset <= limit && count <= limit - offset;
}

static int
payloads(uint disk, struct kernel_info *result)
{
  const struct kernel_info *info = physical(BOUNCE_ADDR);

  if(read_blocks(disk, BOOT_KERNEL_LBA-1, 1) < 0 ||
     info->magic != BOOT_KERNEL_MAGIC || !info->bytes || info->bytes > BOOT_PAYLOAD_MAX)
    return 0;
  *result = *info;
  return 1;
}

static int
check_segments(const struct elfhdr *elf, const struct proghdr *ph, uint bytes)
{
  uint i, j, end, other_end;
  int found = 0;

  for(i = 0; i < elf->phnum; i++){
    const struct proghdr *seg = &ph[i];

    if(seg->type != ELF_PROG_LOAD)
      continue;
    if(seg->paddr < EXTMEM || seg->paddr >= BOOT_KERNEL_LIMIT ||
       seg->memsz > BOOT_KERNEL_LIMIT - seg->paddr ||
       seg->filesz > seg->memsz || !in_file(seg->off, seg->filesz, bytes))
      return 0;
    end = seg->paddr + seg->memsz;
    if((seg->flags & ELF_PROG_FLAG_EXEC) &&
       elf->entry >= seg->paddr && elf->entry < seg->paddr + seg->filesz)
      found = 1;
    for(j = 0; j < i; j++){
      if(ph[j].type != ELF_PROG_LOAD || !ph[j].memsz)
        continue;
      other_end = ph[j].paddr + ph[j].memsz;
      if(seg->memsz && seg->paddr < other_end && ph[j].paddr < end)
        return 0;
    }
  }
  return found;
}

uint
load_kernel(uint disk)
{
  struct kernel_info info;
  struct elfhdr elf;
  struct proghdr ph[MAX_SEGMENTS];
  uint bytes, count, i, j;
  uchar *dst;

  if(!payloads(disk, &info))
    return 0;
  bytes = info.size32 && info.size32 <= info.bytes ? info.size32 : info.bytes;
  if(bytes < sizeof(elf) ||
     read_bytes(disk, &elf, 0, sizeof(elf)) < 0)
    return 0;
  if(elf.magic != ELF_MAGIC || elf.elf[0] != 1 || elf.elf[1] != 1 ||
     elf.type != 2 || elf.machine != 3 || elf.phentsize != sizeof(ph[0]) ||
     !elf.phnum || elf.phnum > MAX_SEGMENTS)
    return 0;
  count = elf.phnum * sizeof(ph[0]);
  if(!in_file(elf.phoff, count, bytes) || read_bytes(disk, ph, elf.phoff, count) < 0 ||
     !check_segments(&elf, ph, bytes))
    return 0;

  for(i = 0; i < elf.phnum; i++){
    if(ph[i].type != ELF_PROG_LOAD)
      continue;
    dst = physical(ph[i].paddr);
    if(read_bytes(disk, dst, ph[i].off, ph[i].filesz) < 0)
      return 0;
    for(j = ph[i].filesz; j < ph[i].memsz; j++)
      dst[j] = 0;
  }
  return elf.entry;
}

static int
payload64(uint disk, struct kernel_info *info)
{
  return payloads(disk, info) && info->size32 && info->size32 <= info->offset64 &&
         info->offset64 % BOOT_SECTOR_SIZE == 0 && info->size64 >= sizeof(struct elf64) &&
         in_file(info->offset64, info->size64, info->bytes);
}

int
boot_has_kernel64(uint disk)
{
  struct kernel_info info;

  return long_mode_available() && payload64(disk, &info);
}

uint
load_kernel64(uint disk)
{
  struct kernel_info info;
  struct elf64 elf;
  struct segment64 ph[MAX_SEGMENTS];
  uint count, start = 0;

  if(!payload64(disk, &info) ||
     read_bytes(disk, &elf, info.offset64, sizeof(elf)) < 0 ||
     elf.magic != ELF_MAGIC || elf.ident[0] != 2 || elf.ident[1] != 1 ||
     elf.ident[2] != 1 || elf.version != 1 || elf.type != 2 || elf.machine != 62 ||
     elf.ehsize != sizeof(elf) || elf.phentsize != sizeof(ph[0]) ||
     !elf.phnum || elf.phnum > MAX_SEGMENTS)
    return 0;
  count = elf.phnum * sizeof(ph[0]);
  if(elf.phoff > info.size64 || !in_file(elf.phoff, count, info.size64) ||
     read_bytes(disk, ph, info.offset64 + elf.phoff, count) < 0)
    return 0;
  for(uint i = 0; i < elf.phnum; i++){
    const struct segment64 *p = &ph[i];
    if(p->type != ELF_PROG_LOAD)
      continue;
    if(p->paddr < EXTMEM || p->paddr >= BOOT_KERNEL_LIMIT ||
       p->memsz > BOOT_KERNEL_LIMIT - p->paddr || p->filesz > p->memsz ||
       p->offset > info.size64 || !in_file(p->offset, p->filesz, info.size64))
      return 0;
    for(uint j = 0; j < i; j++)
      if(ph[j].type == ELF_PROG_LOAD && ph[j].memsz && p->memsz &&
         p->paddr < ph[j].paddr + ph[j].memsz && ph[j].paddr < p->paddr + p->memsz)
        return 0;
    if((p->flags & ELF_PROG_FLAG_EXEC) && p->vaddr == p->paddr &&
       elf.entry >= p->paddr && elf.entry - p->paddr < p->filesz)
      start = elf.entry;
  }
  if(!start)
    return 0;
  for(uint i = 0; i < elf.phnum; i++){
    const struct segment64 *p = &ph[i];
    if(p->type != ELF_PROG_LOAD)
      continue;
    uchar *dst = physical(p->paddr);
    if(read_bytes(disk, dst, info.offset64 + p->offset, p->filesz) < 0)
      return 0;
    for(uint j = p->filesz; j < p->memsz; j++)
      dst[j] = 0;
  }
  return start;
}


int
boot_root(uint disk, int live)
{
  struct boothw *hw = physical(BOOTHW_ADDR);
  const uchar *buffer = physical(BOUNCE_ADDR);
  uint tag, first, count, copied, n, i;
  uchar *dst;

  hw->bios_drive = disk;
  if(read_blocks(disk, 0, 1) < 0 ||
     !install_partition(buffer, &tag, &first, &count))
    return live ? -1 : 0;
  hw->root_tag = tag;
  hw->root_lba = first;
  hw->root_sectors = count;
  if(!live)
    return 0;
  if(count > INSTALL_RAM_MAX / BOOT_SECTOR_SIZE ||
     hw->memend < INSTALL_RAM_BASE + count * BOOT_SECTOR_SIZE + 8U*1024*1024)
    return -1;

  puts_serial("Loading ext2 into memory\n");
  puts_screen("Loading ext2 into memory\n");
  dst = physical(INSTALL_RAM_BASE);
  for(copied = 0; copied < count; copied += n){
    n = count - copied;
    if(n > BOUNCE_BLOCKS)
      n = BOUNCE_BLOCKS;
    if(read_blocks(disk, first + copied, n) < 0)
      return -1;
    for(i = 0; i < n * BOOT_SECTOR_SIZE; i++)
      *dst++ = buffer[i];
  }
  hw->ram_root_bytes = count * BOOT_SECTOR_SIZE;
  return 0;
}
