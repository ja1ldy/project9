/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "layout.h"

.globl start
.code16
start:
  ljmp $0, $setup

setup:
  cli
  xorw %ax, %ax
  movw %ax, %ss
  movw $0x7c00, %sp
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %gs
  cld
  sti
  movb %dl, drive

  # ask the bios for the boot stage from the same disk
  movw $packet, %si
  movw $0x4200, %ax
  int $0x13
  jc failed

  cli
  inb $0x92, %al
  andb $0xfe, %al
  orb $2, %al
  outb %al, $0x92
  lgdt gdtr
  movl %cr0, %eax
  orl $1, %eax
  movl %eax, %cr0
  ljmp $0x8, $protected

failed:
  xorw %ax, %ax
  movw %ax, %ds
  movw $diskerror, %si
1:
  lodsb
  testb %al, %al
  jz 2f
  movb $0x0e, %ah
  movw $7, %bx
  int $0x10
  jmp 1b
2:
  cli
  hlt
  jmp 2b

.code32
protected:
  movw $0x10, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %ss
  movw %ax, %fs
  movw %ax, %gs
  movl $0x7c00, %esp

  # pass the bios disk number and leave the early btx banner muted
  pushl $0
  pushl $0
  pushl $0
  pushl $0
  movzbl drive, %eax
  pushl %eax
  pushl $0x10000
  movl $BOOT_STAGE_ADDR, %eax
  call *%eax
3:
  hlt
  jmp 3b

.balign 8
gdt:
  .quad 0x0000000000000000
  .quad 0x00cf9a000000ffff
  .quad 0x00cf92000000ffff
gdtr:
  .word gdtr - gdt - 1
  .long gdt

.balign 4
packet:
  .byte 16, 0
  .word BOOT_STAGE_BLOCKS
  .word 0, BOOT_STAGE_ADDR >> 4
  .quad 1
drive:
  .byte 0
diskerror:
  .asciz "Project9: cannot read the boot stage"

# leave room for the partition table
.org 440
.org 510
.word 0xaa55
