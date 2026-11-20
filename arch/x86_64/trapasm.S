/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

.text
.globl alltraps
alltraps:
  pushq %rax
  pushq %rcx
  pushq %rdx
  pushq %rbx
  pushq %rbp
  pushq %rsi
  pushq %rdi
  pushq %r8
  pushq %r9
  pushq %r10
  pushq %r11
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15

  xor %eax, %eax
  movw %ds, %ax
  pushq %rax
  movw %es, %ax
  pushq %rax
  movw %fs, %ax
  pushq %rax
  movw %gs, %ax
  pushq %rax

  cld
  movq %rsp, %rbx
  movq %rsp, %rdi
  andq $-16, %rsp
  call trap
  movq %rbx, %rsp

.globl trapret
trapret:
  popq %rax
  movw %ax, %gs
  popq %rax
  movw %ax, %fs
  popq %rax
  movw %ax, %es
  popq %rax
  movw %ax, %ds

  popq %r15
  popq %r14
  popq %r13
  popq %r12
  popq %r11
  popq %r10
  popq %r9
  popq %r8
  popq %rdi
  popq %rsi
  popq %rbp
  popq %rbx
  popq %rdx
  popq %rcx
  popq %rax
  addq $16, %rsp
  iretq

.section .note.GNU-stack,"",@progbits
