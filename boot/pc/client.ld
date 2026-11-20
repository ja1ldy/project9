/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

PHDRS
{
  image PT_LOAD FLAGS(7);
}

SECTIONS
{
  . = 0;
  .text : { *(.text .text.*) } :image
  .rodata : { *(.rodata .rodata.*) } :image
  .data : { *(.data .data.*) } :image
  _edata = .;
  .bss : { *(.bss .bss.* COMMON) } :image
  _end = .;
  /DISCARD/ : { *(.eh_frame .eh_frame.* .note .note.* .comment) }
}
