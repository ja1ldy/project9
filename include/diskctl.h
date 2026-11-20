/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_DISKCTL_H
#define P9_DISKCTL_H

#define DC_VOLUME 3
#define DC_MAX 36
#define DC_SECTOR 512
#define DC_START 10240
#define DC_MIN 16384
#define DC_LIMIT 262144
#define DC_MARKER "P9INSTALLSCRATCH"
#define DC_MARKER_LEN 16

#define DC_INFO 0
#define DC_BOOT 1
#define DC_READ_PART 2
#define DC_WRITE_PART 3
#define DC_MBR 4
#define DC_BOOTBLOCK 5
#define DC_FINISH 6
#define DC_READ_MBR 7
#define DC_SEED 8
#define DC_STATS 9
#define DC_EXT2_STATS 10

#define DC_ATA 0
#define DC_AHCI 1
#define DC_BOOT_DISK 1
#define DC_ACTIVE 2
#define DC_TEST 4

struct diskinfo {
  unsigned int sectors, first, count, flags;
  unsigned int type, index;
};

struct diskstats {
  unsigned int reads, writes, read_sectors, written_sectors, flushes, errors;
};

struct ext2stats {
  unsigned int hits, misses, skipped_writes;
};

struct diskboot {
  unsigned int drive, live, identified;
};

#endif
