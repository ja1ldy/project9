/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

.text
.code16
.globl start
start:
  cli
  xorw %ax, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %ss
  lgdt gdtr
  movl %cr0, %eax
  orl $1, %eax
  movl %eax, %cr0
  ljmpl $24, $protected_entry

.code32
protected_entry:
  movw $16, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %ss
  movl %cr4, %eax
  orl $0x20, %eax
  movl %eax, %cr4
  movl start-24, %eax
  movl %eax, %cr3
  movl $0xc0000080, %ecx
  rdmsr
  orl $0x100, %eax
  wrmsr
  movl %cr0, %eax
  orl $0x80010000, %eax
  movl %eax, %cr0
  ljmp $8, $long_entry

.balign 8
gdt:
  .quad 0
  .quad 0x00209a0000000000
  .quad 0x00cf92000000ffff
  .quad 0x00cf9a000000ffff
gdt_end:
gdtr:
  .word gdt_end - gdt - 1
  .long gdt

.code64
long_entry:
  movq start-8, %rsp
  xor %ebp, %ebp
  movq start-16, %rax
  call *%rax
  ud2

.section .note.GNU-stack,"",@progbits
