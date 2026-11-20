/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

# Context switch
#
#   void swtch(struct context **old, struct context *new);
# 
# save the current registers on the stack creating
# a struct context and save its address in old
# switch stacks to new and pop previously saved registers

.globl swtch
swtch:
  movl 4(%esp), %eax
  movl 8(%esp), %edx

  # save old callee saved registers
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  # Switch stacks
  movl %esp, (%eax)
  movl %edx, %esp

  # load new callee saved registers
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
