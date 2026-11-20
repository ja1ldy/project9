/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_BOOTHW_H
#define PROJECT9_BOOTHW_H

#define BOOTHW_ADDR 0x30000
#define BOOTHW_MAGIC 0x50394857U
#define BOOT_FB_MAX_WIDTH 1024
#define BOOT_FB_MAX_HEIGHT 768
#define BOOT_FB_MAX_STRIDE 8192

struct boothw {
  unsigned int magic, memend;
  unsigned int fbaddr, width, height, stride;
  unsigned int root_tag, root_lba, root_sectors, ram_root_bytes, bios_drive;
};

#endif
