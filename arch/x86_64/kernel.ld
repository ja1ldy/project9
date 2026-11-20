/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(start32)

PHDRS
{
  bootstrap PT_LOAD FLAGS(5);
  scratch PT_LOAD FLAGS(6);
  kernel PT_LOAD FLAGS(5);
  writable PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = 0x100000;
  .boot : { *(.boot) } :bootstrap
  . = ALIGN(4096);
  .bootbss (NOLOAD) : { *(.bootbss) } :scratch

  high = 0xffffffff80000000;
  . += high;
  .text : AT(ADDR(.text) - high) { *(.text .text.*) } :kernel
  .rodata : AT(ADDR(.rodata) - high) { *(.rodata .rodata.*) } :kernel
  . = ALIGN(4096);
  data = .;
  .data : AT(ADDR(.data) - high) { *(.data .data.*) } :writable
  .bss : AT(ADDR(.bss) - high) { *(.bss .bss.* COMMON) } :writable
  end = .;

  ASSERT(. - high <= 0x380000, "amd64 bootstrap needs early memory for page tables")
  /DISCARD/ : { *(.eh_frame .note .note.* .comment) }
}
