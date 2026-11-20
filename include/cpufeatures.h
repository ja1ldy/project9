/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_CPUFEATURES_H
#define P9_CPUFEATURES_H

#define X86_FXSR    (1U << 24)
#define X86_SSE     (1U << 25)
#define X86_SSE2    (1U << 26)
#define X86_XSAVE   (1U << 26)
#define X86_OSXSAVE (1U << 27)
#define X86_AVX     (1U << 28)

static inline int
x86_has_cpuid(void)
{
  unsigned long old, changed;
  asm volatile("pushf; pop %0; mov %0,%1; xor $0x200000,%1;"
               "push %1; popf; pushf; pop %1; push %0; popf"
               : "=&r"(old), "=&r"(changed) : : "cc");
  return ((old ^ changed) & 0x200000) != 0;
}

static inline void
x86_cpuid(unsigned int leaf, unsigned int subleaf, unsigned int out[4])
{
  asm volatile("cpuid" : "=a"(out[0]), "=b"(out[1]),
               "=c"(out[2]), "=d"(out[3]) : "a"(leaf), "c"(subleaf));
}

#endif
