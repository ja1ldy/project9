/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "diskuser.h"

int
main(int argc, char **argv)
{
  struct diskinfo disk;
  uchar mbr[DC_SECTOR];
  int id, write = argc == 3 && !strcmp(argv[1], "-iy");
  uint count;

  if((argc != 2 && !write) || (id = diskid(argv[argc-1], 0)) < 0){
    printf(2, "usage: fdisk [-iy] disk\n");
    return 2;
  }
  if(diskctl(DC_INFO, id, 0, 1, &disk) < 0){
    printf(2, "fdisk: disk unavailable\n");
    return 1;
  }
  if(!write){
    printf(1, "%u sectors; partition 1 starts at %u, %u sectors\n",
           disk.sectors, disk.first, disk.count);
    return 0;
  }
  if(!(disk.flags & DC_TEST) || disk.sectors <= DC_START + DC_MIN){
    printf(2, "fdisk: target is protected or too small\n");
    return 1;
  }
  count = disk.sectors - DC_START - 1;
  if(count > DC_LIMIT)
    count = DC_LIMIT;
  count &= ~7U;
  memset(mbr, 0, sizeof(mbr));
  mbr[446] = 0x80;
  mbr[447] = mbr[451] = 0xfe;
  mbr[448] = mbr[449] = mbr[452] = mbr[453] = 0xff;
  mbr[450] = 0x83;
  dput(mbr+454, DC_START, 4);
  dput(mbr+458, count, 4);
  mbr[510] = 0x55;
  mbr[511] = 0xaa;
  if(diskctl(DC_MBR, id, 0, 1, mbr) < 0){
    printf(2, "fdisk: could not write the partition table\n");
    return 1;
  }
  printf(1, "partition 1 starts at %u and contains %u sectors\n", DC_START, count);
  return 0;
}
