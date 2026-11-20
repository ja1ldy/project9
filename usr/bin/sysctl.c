/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "diskuser.h"
#include "devices.h"

static void
name(struct diskinfo *disk)
{
  printf(1, "%s%d", disk->type == DC_AHCI ? "sd" : "hd", disk->index);
}

int
main(int argc, char **argv)
{
  struct diskinfo disk;
  struct diskboot boot;
  int i, first = 1;
  struct diskstats io;
  struct ext2stats cache;

  if(argc == 2 && !strcmp(argv[1], "hw.machine")){
    int width = cpuquery(3);
    if(width != 32 && width != 64)
      return 1;
    printf(1, "hw.machine=%s\n", width == 64 ? "x86_64" : "i386");
  } else if(argc == 2 && !strcmp(argv[1], "hw.ncpu")){
    int cpus = cpuquery(1);
    if(cpus < 1)
      return 1;
    printf(1, "hw.ncpu=%d\n", cpus);
  } else if(argc == 2 && !strcmp(argv[1], "hw.usb")){
    int online = usbquery(1);
    if(online < 0)
      return 1;
    printf(1, "hw.usb=");
    for(i = 0; i < USB_DISK_COUNT; i++)
      if(online & (1U << i)){
        printf(1, "%susb%d", first ? "" : ",", i);
        first = 0;
      }
    printf(1, "\n");
  } else if(argc == 2 && !strcmp(argv[1], "kern.diskstats") &&
     diskctl(DC_STATS, 0, 0, 1, &io) == 0){
    printf(1, "kern.diskstats reads=%u read_sectors=%u writes=%u written_sectors=%u flushes=%u errors=%u\n",
           io.reads, io.read_sectors, io.writes, io.written_sectors, io.flushes, io.errors);
  } else if(argc == 2 && !strcmp(argv[1], "vfs.ext2.stats") &&
            diskctl(DC_EXT2_STATS, 0, 0, 1, &cache) == 0){
    printf(1, "vfs.ext2.stats cache_hits=%u cache_misses=%u skipped_writes=%u\n",
           cache.hits, cache.misses, cache.skipped_writes);
  } else if(argc == 2 && !strcmp(argv[1], "hw.disknames")){
    printf(1, "hw.disknames=");
    for(i = 0; i < DC_MAX; i++){
      if(diskctl(DC_INFO, i, 0, 1, &disk) < 0)
        continue;
      if(!first)
        printf(1, ",");
      first = 0;
      name(&disk);
      if(disk.flags & DC_BOOT_DISK)
        printf(1, ":boot");
      if(disk.flags & DC_ACTIVE)
        printf(1, ":active");
      if(disk.flags & DC_TEST)
        printf(1, ":test-target");
    }
    printf(1, "\n");
  } else if(argc == 2 && (!strcmp(argv[1], "hw.bootdisk") || !strcmp(argv[1], "hw.live")) &&
            diskctl(DC_BOOT, 0, 0, 1, &boot) == 0){
    if(!strcmp(argv[1], "hw.live"))
      printf(1, "hw.live=%d\n", boot.live);
    else if(boot.identified == 1){
      for(i = 0; i < DC_MAX; i++)
        if(diskctl(DC_INFO, i, 0, 1, &disk) == 0 && disk.flags & DC_BOOT_DISK){
          printf(1, "hw.bootdisk=");
          name(&disk);
          printf(1, "\n");
          break;
        }
    } else
      printf(1, "hw.bootdisk=bios:0x%x\n", boot.drive);
  } else {
    printf(2, "usage: sysctl hw.machine | hw.ncpu | hw.usb | hw.disknames | hw.bootdisk | hw.live | kern.diskstats | vfs.ext2.stats\n");
    return 1;
  }
  return 0;
}
