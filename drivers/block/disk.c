/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "boothw.h"
#include "defs.h"
#include "disk.h"
#include "diskctl.h"
#include "p9abi.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "ext2.h"
#include "install.h"
#include "memlayout.h"
#include "param.h"
#include "pci.h"

#define SECTOR 512
enum { ATA, MEMORY, AHCI };

static struct volume_disk {
  uint backend, index, first, count;
} disks[INSTALLDEV+1];

static struct sleeplock io;
static struct diskinfo physical[DC_MAX];
static int selected = -1;
static int probed;

static int target_ok(uint);

static struct diskstats stats;

static int
transfer(uint id, uint sector, uint n, void *buf, int write)
{
  uchar *data = buf;

  if(id >= DC_MAX || !n || n > DISK_BATCH ||
     sector >= physical[id].sectors || n > physical[id].sectors-sector)
    return -1;

  while(n){
    uint batch = id >= 4 && n > AHCI_BATCH ? AHCI_BATCH : n;
    int result = id < 4 ? ataexttransfer(id, sector, batch, data, write) :
                 ahcitransfer(id-4, sector, batch, data, write);
    if(write){
      stats.writes++;
      stats.written_sectors += batch;
    } else {
      stats.reads++;
      stats.read_sectors += batch;
    }
    if(result < 0){
      stats.errors++;
      return -1;
    }
    sector += batch;
    n -= batch;
    data += batch*SECTOR;
  }
  return 0;
}

static int
flush(uint id)
{
  int result = id < 4 ? ataextflush(id) : ahciflush(id-4);

  stats.flushes++;
  if(result < 0)
    stats.errors++;
  return result;
}

static int
sectorio(uint id, uint sector, void *buf, int write)
{
  return transfer(id, sector, 1, buf, write);
}

static int
layout(const uchar *mbr, uint limit, uint *first, uint *count)
{
  const uchar *p = mbr + 446;
  uint i;

  if(mbr[510] != 0x55 || mbr[511] != 0xaa || p[4] != INSTALL_PART_TYPE ||
     (p[0] != 0 && p[0] != 0x80))
    return 0;
  for(i = 462; i < 510; i++)
    if(mbr[i])
      return 0;
  *first = install_dword(p+8);
  *count = install_dword(p+12);
  return *first && *first < limit && *count && *count <= limit-*first;
}

static int
emptyboot(const uchar *mbr)
{
  for(uint i = 0; i < INSTALL_PART_OFFSET; i++)
    if(mbr[i])
      return 0;
  return 1;
}

void
diskdiscover(void)
{
  const struct boothw *hw = P2V(BOOTHW_ADDR);
  uint id, tag, first, count;
  uchar report[DC_SECTOR];
  struct diskinfo *p;

  initsleeplock(&io, "disk operations");
  for(id = 0; id < DC_MAX && id < 4+ahcidisks(); id++){
    p = &physical[id];
    p->type = id >= 4;
    p->index = id >= 4 ? id-4 : id;
    p->sectors = p->type ? ahciprobe(p->index) : ataextprobe(id);
    if(!p->sectors || sectorio(id, 0, report, 0) < 0)
      continue;
    if(install_partition(report, &tag, &first, &count)){
      if(hw->magic == BOOTHW_MAGIC && tag == hw->root_tag &&
         first == hw->root_lba && count == hw->root_sectors)
        p->flags |= DC_BOOT_DISK;
      if(first < p->sectors && count <= p->sectors-first){
        p->first = first;
        p->count = count;
      }
      continue;
    }
    if(!layout(report, p->sectors, &p->first, &p->count))
      p->first = p->count = 0;
    if(!emptyboot(report) ||
       (p->first && (p->first != DC_START || p->count < DC_MIN ||
                     p->count > DC_LIMIT || p->count >= p->sectors-p->first)))
      continue;
    if(!p->first){
      int blank = 1;
      for(uint i = INSTALL_PART_OFFSET; i < DC_SECTOR; i++)
        if(report[i])
          blank = 0;
      if(!blank)
        continue;
    }
    if(hw->magic == BOOTHW_MAGIC && hw->ram_root_bytes &&
       p->sectors >= DC_START + DC_MIN + 1 &&
       sectorio(id, p->sectors-1, report, 0) == 0 &&
       !memcmp(report, DC_MARKER, DC_MARKER_LEN))
      p->flags |= DC_TEST;
  }
  probed = 1;
}

uint
diskprobe(uint dev)
{
  const struct boothw *hw = P2V(BOOTHW_ADDR);
  if(dev < ROOTDEV || dev > INSTALLDEV)
    return 0;
  struct volume_disk *d = &disks[dev];
  uchar mbr[SECTOR];

  if(!probed)
    diskdiscover();
  if(dev == INSTALLDEV)
    return d->count;

  if(dev == EXT2DEV && hw->magic == BOOTHW_MAGIC && hw->ram_root_bytes)
    return 0;
  uint i, sectors, tag, first, count, match = DC_MAX;

  if(dev == ROOTDEV && hw->magic == BOOTHW_MAGIC && hw->root_lba){
    if(hw->ram_root_bytes){
      d->backend = MEMORY;
      d->count = hw->ram_root_bytes / SECTOR;
      return d->count;
    }
    for(i = 0; i < 4 + ahcidisks(); i++){
      sectors = physical[i].sectors;
      if(sectors <= hw->root_lba || hw->root_sectors > sectors - hw->root_lba ||
         sectorio(i, 0, mbr, 0) < 0 ||
         !install_partition(mbr, &tag, &first, &count) ||
         tag != hw->root_tag || first != hw->root_lba || count != hw->root_sectors)
        continue;
      if(match != DC_MAX){
        cprintf("disk: more than one disk matches the boot partition\n");
        return 0;
      }
      match = i;
    }
    if(match == DC_MAX)
      return 0;
    *d = (struct volume_disk){ match >= 4 ? AHCI : ATA, physical[match].index,
                              hw->root_lba, hw->root_sectors };
    physical[match].flags |= DC_ACTIVE;
    cprintf("disk: install partition on %s %d\n", match >= 4 ? "ahci" : "ata", d->index);
    return d->count;
  }
  if(dev == EXT2DEV && disks[ROOTDEV].count &&
     disks[ROOTDEV].backend == ATA && disks[ROOTDEV].index == EXT2DEV)
    return 0;
  if(physical[dev].sectors)
    physical[dev].flags |= DC_ACTIVE;
  *d = (struct volume_disk){ ATA, dev, 0, physical[dev].sectors };
  return d->count;
}

// check the live attachment and its bounds while holding the disk lock
static int
validvolume(uint dev, struct volume_disk *d, uint *id)
{
  if(!d->count)
    return 0;
  if(d->backend == MEMORY)
    return dev == ROOTDEV && d->count <= INSTALL_RAM_MAX/SECTOR;
  *id = d->index + (d->backend == AHCI ? 4 : 0);
  if(*id >= DC_MAX || d->first >= physical[*id].sectors ||
     d->count > physical[*id].sectors-d->first)
    return 0;
  return dev != INSTALLDEV || (selected == (int)*id && target_ok(*id) &&
         d->first == DC_START && d->first == physical[*id].first &&
         d->count == physical[*id].count && d->count < physical[*id].sectors-d->first);
}

int
diskrw(uint dev, uint sector, uint n, void *buffer, int writing)
{
  struct volume_disk *d;
  uint id;
  int result = -1;

  if(dev < ROOTDEV || dev > INSTALLDEV || !n || n > DISK_BATCH)
    return -1;
  d = &disks[dev];
  // the ram root is fixed for the lifetime of this boot
  if(dev == ROOTDEV && d->backend == MEMORY){
    if(!validvolume(dev, d, &id) || sector >= d->count || n > d->count-sector)
      return -1;
    uchar *ram = P2V(INSTALL_RAM_BASE + sector * SECTOR);
    if(writing)
      memmove(ram, buffer, n*SECTOR);
    else
      memmove(buffer, ram, n*SECTOR);
    return 0;
  }
  acquiresleep(&io);
  if(validvolume(dev, d, &id) && sector < d->count && n <= d->count-sector)
    result = transfer(id, d->first + sector, n, buffer, writing);
  releasesleep(&io);
  return result;
}

int
diskflush(uint dev)
{
  struct volume_disk *d;
  uint id;
  int result = -1;

  if(dev < ROOTDEV || dev > INSTALLDEV)
    return -1;
  d = &disks[dev];
  if(dev == ROOTDEV && d->backend == MEMORY)
    return 0;
  acquiresleep(&io);
  if(validvolume(dev, d, &id))
    result = flush(id);
  releasesleep(&io);
  return result;
}

int
diskislive(void)
{
  return disks[ROOTDEV].backend == MEMORY;
}


static int
target_ok(uint id)
{
  const struct diskinfo *p = &physical[id];

  return diskislive() && (p->flags & DC_TEST) && !(p->flags & (DC_ACTIVE | DC_BOOT_DISK));
}

int
diskselect(uint id)
{
  struct diskinfo *p;
  int result = -1;

  if(id >= DC_MAX)
    return -1;
  acquiresleep(&io);
  p = &physical[id];
  if(selected < 0 && target_ok(id) && p->first == DC_START &&
     p->count >= DC_MIN && p->count <= DC_LIMIT &&
     p->first < p->sectors && p->count < p->sectors-p->first){
    selected = id;
    disks[INSTALLDEV] = (struct volume_disk){id < 4 ? ATA : AHCI, p->index, p->first, p->count};
    result = 0;
  }
  releasesleep(&io);
  return result;
}

void
diskdetach(void)
{
  acquiresleep(&io);
  selected = -1;
  disks[INSTALLDEV].count = 0;
  releasesleep(&io);
}

static int
bootroot(uint id, uint first, uint count, uint tag, uchar *sector)
{
  uint crc = ~0U;

  if(count % 8 || sectorio(id, first+2, sector, 0) < 0 ||
     sector[56] != 0x53 || sector[57] != 0xef ||
     sector[58] != 1 || sector[59] ||
     install_dword(sector+24) != 2 || install_dword(sector+4) != count/8)
    return 0;
  for(int i = 104; i < 120; i++){
    crc ^= sector[i];
    for(int bit = 0; bit < 8; bit++)
      crc = (crc >> 1) ^ (0xedb88320U & -(crc & 1));
  }
  return ~crc == tag;
}

int
diskcommand(int op, uint id, uint start, uint n, void *buf)
{
  const struct boothw *hw = P2V(BOOTHW_ADDR);
  struct diskinfo *p;
  uint offset, first, count, tag;
  int result = -P9_EINVAL;
  uchar sector[DC_SECTOR], mbr[DC_SECTOR];

  if(!probed || id >= DC_MAX)
    return -P9_EINVAL;
  acquiresleep(&io);
  p = &physical[id];
  if(op == DC_BOOT){
    struct diskboot report = {hw->bios_drive, diskislive(), 0};
    for(uint i = 0; i < DC_MAX; i++)
      if(physical[i].flags & DC_BOOT_DISK)
        report.identified++;
    memmove(buf, &report, sizeof(report));
    result = 0;
    goto done;
  }
  if(op == DC_STATS){
    memmove(buf, &stats, sizeof(stats));
    result = 0;
    goto done;
  }
  if(!p->sectors){
    result = -P9_ENODEV;
    goto done;
  }
  if(op == DC_INFO){
    memmove(buf, p, sizeof(*p));
    result = 0;
    goto done;
  }
  if(!diskislive()){
    result = -P9_EPERM;
    goto done;
  }
  if(op == DC_READ_MBR){
    result = sectorio(id, 0, buf, 0) < 0 ? -P9_EIO : 0;
    goto done;
  }

  // raw commands never modify the boot disk or an open filesystem
  if(op != DC_READ_PART && !target_ok(id)){
    result = -P9_EPERM;
    goto done;
  }
  if(selected == (int)id){
    result = -P9_EBUSY;
    goto done;
  }
  if(op == DC_SEED){
    if(sectorio(id, p->sectors-1, sector, 0) == 0){
      memmove(buf, sector+DC_MARKER_LEN, 32);
      result = 0;
    } else
      result = -P9_EIO;
    goto done;
  }
  if(op == DC_MBR || op == DC_FINISH){
    memmove(mbr, buf, sizeof(mbr));
    if(!layout(mbr, p->sectors - 1, &first, &count) ||
       first != DC_START || count < DC_MIN || count > DC_LIMIT)
      goto done;
    if(op == DC_MBR){
      // partition creation has no boot code or disk signature
      if(!emptyboot(mbr))
        goto done;
    } else {
      if(first != p->first || count != p->count ||
         !install_partition(mbr, &tag, &first, &count) ||
         !bootroot(id, first, count, tag, sector))
        goto done;
    }
    if(sectorio(id, 0, mbr, 1) < 0 || flush(id) < 0){
      p->flags &= ~DC_TEST;
      result = -P9_EIO;
      goto done;
    }
    p->first = first;
    p->count = count;
    if(op == DC_FINISH)
      p->flags &= ~DC_TEST;
    result = 0;
    goto done;
  }
  if(op == DC_BOOTBLOCK){
    if(p->first != DC_START || !start || start >= p->first || n > p->first-start)
      goto done;
    offset = start;
  } else if(op == DC_WRITE_PART || op == DC_READ_PART){
    if(!p->count || start >= p->count || n > p->count-start)
      goto done;
    if(op == DC_WRITE_PART && (p->first != DC_START || p->count >= p->sectors-p->first))
      goto done;
    offset = p->first + start;
  } else
    goto done;
  if(transfer(id, offset, n, buf, op != DC_READ_PART) < 0 ||
     (op != DC_READ_PART && flush(id) < 0)){
    result = -P9_EIO;
    goto done;
  }
  result = 0;
done:
  releasesleep(&io);
  return result;
}
