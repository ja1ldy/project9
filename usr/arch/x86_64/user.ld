/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
  code PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = 0;
  .text : { . += 16; *(.text .text.* .rodata .rodata.*) } :code
  . = ALIGN(4096);
  .data : { *(.data .data.*) } :data
  .bss : { *(.bss .bss.* COMMON) } :data
  /DISCARD/ : { *(.eh_frame .note.* .comment) }
}
