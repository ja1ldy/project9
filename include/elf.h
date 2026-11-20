/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr {
  uint magic;  // must equal ELF_MAGIC
  uchar elf[12];
  ushort type;
  ushort machine;
  uint version;
  uint entry;
  uint phoff;
  uint shoff;
  uint flags;
  ushort ehsize;
  ushort phentsize;
  ushort phnum;
  ushort shentsize;
  ushort shnum;
  ushort shstrndx;
};

// Program section header
struct proghdr {
  uint type;
  uint off;
  uint vaddr;
  uint paddr;
  uint filesz;
  uint memsz;
  uint flags;
  uint align;
};

#ifdef P9_KERNEL64
struct elfhdr64 {
  uint magic;
  uchar elf[12];
  ushort type, machine;
  uint version;
  uptr entry, phoff, shoff;
  uint flags;
  ushort ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};

struct proghdr64 {
  uint type, flags;
  uptr off, vaddr, paddr, filesz, memsz, align;
};

_Static_assert(sizeof(struct elfhdr64) == 64 && sizeof(struct proghdr64) == 56,
               "elf64 header sizes");
#endif

// Values for Proghdr type
#define ELF_PROG_LOAD           1
#define ELF_PROG_DYNAMIC        2
#define ELF_PROG_INTERP         3

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4
