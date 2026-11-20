/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

.text
.globl _start
_start:
  xor %ebp, %ebp
  movq (%rsp), %rdi
  leaq 8(%rsp), %rsi
  leaq 8(%rsi,%rdi,8), %rdx
  movq %rdx, environ(%rip)
  call main
  movl %eax, %edi
#ifdef P9_LIBC_START
  call p9_exit
#else
  call exitstatus
#endif
  ud2

/* the handler returns here with the stack aligned for the next call */
.globl p9_sigtramp
p9_sigtramp:
  call sigreturn
  call exit
  ud2

.section .note.GNU-stack,"",@progbits
