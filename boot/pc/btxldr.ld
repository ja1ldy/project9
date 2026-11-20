/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(start)

PHDRS
{
  code PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = boot_stage_addr;
  .text : { *(.text .text.*) } :code
  .data : { *(.rodata .rodata.*) *(.data .data.*) } :data
  .bss (NOLOAD) : { *(.bss .bss.* COMMON) } :data
  /DISCARD/ : { *(.note .note.* .eh_frame .eh_frame.* .comment) }
}

/* btx looks for its runtime exactly one page after the loader starts */
ASSERT(start == boot_stage_addr, "BTX loader entry must start the boot stage")
ASSERT(ADDR(.bss) + SIZEOF(.bss) <= boot_stage_addr + 0x1000,
       "BTX loader must fit before the runtime")
