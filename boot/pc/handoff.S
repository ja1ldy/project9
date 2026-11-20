/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

.text
.globl kernel_jump
kernel_jump:
  # btx has returned us to flat privileged segments
  cli
  cld
  movl 4(%esp), %eax
  jmp *%eax
