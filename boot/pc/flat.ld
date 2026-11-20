/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(start)

PHDRS
{
  code PT_LOAD FLAGS(5);
}

SECTIONS
{
  .text boot_origin : { *(.text .text.*) } :code
  .unexpected : { *(.rodata .rodata.* .data .data.* .bss .bss.* COMMON) } :code
  /DISCARD/ : { *(*) }
}

/* keep only the bytes the bios and btx expect to find on disk */
ASSERT(start == boot_origin, "boot code must start at its load address")
ASSERT(SIZEOF(.unexpected) == 0, "flat boot code must keep its data in text")
ASSERT(boot_size == 0 || SIZEOF(.text) == boot_size, "boot sector must be 512 bytes")
