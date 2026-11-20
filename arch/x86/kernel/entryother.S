/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
	
# Each non-boot CPU ("AP") is started up in response to a STARTUP
# IPI from the boot CPU.  Section B.4.2 of the Multi-Processor
# Specification says that the AP will start in real mode with CS:IP
# set to XY00:0000, where XY is an 8-bit value sent with the
# STARTUP. Thus this code must start at a 4096-byte boundary.
#
# Because this code sets DS to zero, it must sit
# at an address in the low 2^16 bytes.
#
# Startothers (in main.c) sends the STARTUPs one at a time.
# It copies this code (start) at 0x7000.  It puts the address of
# a newly allocated per-core stack in start-4,the address of the
# place to jump to (mpenter) in start-8, and the physical address
# of entrypgdir in start-12.
#
# this code combines elements of bootasm s and entry s

.code16           
.globl start
start:
  cli            

  # Zero data segment registers DS, ES, and SS.
  xorw    %ax,%ax
  movw    %ax,%ds
  movw    %ax,%es
  movw    %ax,%ss

  # Switch from real to protected mode.  Use a bootstrap GDT that makes
  # virtual addresses map directly to physical addresses so that the
  # effective memory map doesn't change during the transition.
  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE, %eax
  movl    %eax, %cr0

  # complete the transition to 32 bit protected mode by using a long jmp
  # to reload %cs and %eip.  The segment descriptors are set up with no
  # translation, so that the mapping is still the identity mapping.
  ljmpl    $(SEG_KCODE<<3), $(start32)

//PAGEBREAK!
.code32  # tell assembler to generate 32 bit code now
start32:
  # Set up the protected-mode data segment registers
  movw    $(SEG_KDATA<<3), %ax    # our data segment selector
  movw    %ax, %ds                # to ds data segment
  movw    %ax, %es                # to es extra segment
  movw    %ax, %ss                # to ss stack segment
  movw    $0, %ax                 # zero segments not ready for use
  movw    %ax, %fs                # to fs
  movw    %ax, %gs                # to gs

  # Turn on page size extension for 4Mbyte pages
  movl    %cr4, %eax
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4
  # use entrypgdir as our initial page table
  movl    (start-12), %eax
  movl    %eax, %cr3
  # Turn on paging.
  movl    %cr0, %eax
  orl     $(CR0_PE|CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

  # Switch to the stack allocated by startothers()
  movl    (start-4), %esp
  # Call mpenter()
  call	 *(start-8)

  movw    $0x8a00, %ax
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax
  outw    %ax, %dx
spin:
  jmp     spin

.p2align 2
gdt:
  SEG_NULLASM
  SEG_ASM(STA_X|STA_R, 0, 0xffffffff)
  SEG_ASM(STA_W, 0, 0xffffffff)


gdtdesc:
  .word   (gdtdesc - gdt - 1)
  .long   gdt

