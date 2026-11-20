/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_TRAPFRAME_H
#define PROJECT9_TRAPFRAME_H
#ifdef P9_KERNEL64
struct trapframe {
  uptr gs, fs, es, ds;
  uptr r15, r14, r13, r12, r11, r10, r9, r8;
  uptr edi, esi, ebp, ebx, edx, ecx, eax;
  uptr trapno, err, eip, cs, eflags, esp, ss;
};
#endif

#ifndef P9_KERNEL64
#define trapframe32 trapframe
#endif
struct trapframe32 {
  // general registers in the order saved by pusha
  uint edi;
  uint esi;
  uint ebp;
  uint oesp;      // pusha saves this slot and popa skips it
  uint ebx;
  uint edx;
  uint ecx;
  uint eax;

  // segment registers and the trap number saved by the entry code
  ushort gs;
  ushort padding1;
  ushort fs;
  ushort padding2;
  ushort es;
  ushort padding3;
  ushort ds;
  ushort padding4;
  uint trapno;

  // the processor saves these fields on entry
  uint err;
  uint eip;
  ushort cs;
  ushort padding5;
  uint eflags;

  // the processor saves these only when the privilege level changes
  uint esp;
  ushort ss;
  ushort padding6;
};
#endif
