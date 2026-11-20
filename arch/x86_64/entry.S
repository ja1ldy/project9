/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */
/* adapted from entry64.S by Brian Swetland
 * see Documentation/xv6-x86_64-LICENSE.txt for the upstream terms
 */


.section .boot,"ax"
.code32
.globl start32
start32:
  cli
  cld

  # the loader has already cleared these private page tables
  movl $(pdpt + 3), bootstrap_pml4
  movl $(pdpt + 3), bootstrap_pml4 + 511*8
  movl $(directory + 3), pdpt
  movl $(directory + 3), pdpt + 510*8
  movl $0x83, directory
  movl $0x200083, directory + 8

  lgdt gdtr
  movl %cr4, %eax
  orl $0x20, %eax
  movl %eax, %cr4
  movl $bootstrap_pml4, %eax
  movl %eax, %cr3
  movl $0xc0000080, %ecx
  rdmsr
  orl $0x100, %eax
  wrmsr
  movl %cr0, %eax
  orl $0x80010000, %eax
  movl %eax, %cr0
  ljmp $8, $start64

.balign 8
gdt:
  .quad 0
  .quad 0x00209a0000000000
  .quad 0x0000920000000000
gdt_end:
gdtr:
  .word gdt_end - gdt - 1
  .long gdt

.code64
start64:
  movl $16, %eax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %ss
  xor %eax, %eax
  movw %ax, %fs
  movw %ax, %gs
  xor %ebp, %ebp
  movabs $stack_top, %rsp
  movabs $main, %rax
  call *%rax
  ud2

.section .bootbss,"aw",@nobits
.balign 4096
.globl bootstrap_pml4
bootstrap_pml4:
  .skip 4096
pdpt:
  .skip 4096
directory:
  .skip 4096

.section .bss,"aw",@nobits
.balign 4096
  .skip 4096
stack_top:

.section .note.GNU-stack,"",@progbits
