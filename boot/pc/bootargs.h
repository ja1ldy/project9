/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */
#ifndef PROJECT9_BOOT_ARGS_H
#define PROJECT9_BOOT_ARGS_H

// these offsets are the interface between btx and our boot code
#define BOOTARGS_SIZE 24
#define BA_BOOTFLAGS 8
#define BA_BOOTINFO 20
#define BI_SIZE 48
#define KARGS_FLAGS_EXTARG 8

#define ARGSPACE 0x1000
#define ARGOFF 0x800
#define ARGADJ (ARGSPACE - ARGOFF)

#ifndef __ASSEMBLER__
#include "types.h"

struct boot_args {
  uint howto, bios_disk, flags;
  uint spare[2];
  uint bootinfo;
};

_Static_assert(sizeof(struct boot_args) == BOOTARGS_SIZE, "bad btx argument layout");
#endif

#endif
