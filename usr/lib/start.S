/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

.text
.globl _start
_start:
  movl %esp, %ecx
  movl 12(%ecx), %eax
  movl %eax, environ
  andl $-16, %esp
  subl $4, %esp
  pushl %eax
  pushl 8(%ecx)
  pushl 4(%ecx)
  call main
  movl %eax, (%esp)
  call exitstatus
.section .note.GNU-stack,"",@progbits
