/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_FAT_H
#define PROJECT9_FAT_H

#define P9_FAT_MAGIC 0x42463950U
#define P9_FAT_VERSION 1
#define P9_FAT_ALIGN 16

struct fatslice {
  uint offset, size;
};

struct fatheader {
  uint magic, version;
  struct fatslice slice32, slice64;
};

#endif
