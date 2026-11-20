/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "cpufeatures.h"
#include "xstate.h"

enum fpformat { X87, FXSAVE, XSAVE };
static enum fpformat format;
static int initialized;

struct state {
  uchar bytes[XSTATE_BYTES];
} __attribute__((aligned(XSTATE_ALIGN)));

static struct state initial;

void
fpuinit(void)
{
  uint flags[4], max = 0, features = 0, extensions = 0;
  uptr cr0, cr4;
  enum fpformat local = X87;

  if(x86_has_cpuid()){
    x86_cpuid(0, 0, flags);
    max = flags[0];
  }
  if(max >= 1){
    x86_cpuid(1, 0, flags);
    features = flags[3];
    extensions = flags[2];
  }
  if((features & (X86_FXSR | X86_SSE)) == (X86_FXSR | X86_SSE)){
    local = FXSAVE;
    if(max >= 13 && (extensions & (X86_XSAVE | X86_AVX)) == (X86_XSAVE | X86_AVX)){
      x86_cpuid(13, 0, flags);
      if((flags[0] & 7) == 7){
        x86_cpuid(13, 2, flags);
        if(flags[0] == 256 && flags[1] == 576)
          local = XSAVE;
      }
    }
  }

  // the boot processor selects the context layout for every process
  if(!initialized)
    format = local;
  if(local < format)
    panic("incompatible floating point state on this cpu");
  asm volatile("mov %%cr0,%0" : "=r"(cr0));
  cr0 = (cr0 & ~0x0cU) | 0x22;
  asm volatile("mov %0,%%cr0" : : "r"(cr0) : "memory");
  asm volatile("mov %%cr4,%0" : "=r"(cr4));
  if(format >= FXSAVE)
    cr4 |= (1U << 9) | (1U << 10);
  if(format == XSAVE)
    cr4 |= 1U << 18;
  asm volatile("mov %0,%%cr4" : : "r"(cr4) : "memory");
  if(format == XSAVE)
    asm volatile("xsetbv" : : "a"(7), "d"(0), "c"(0) : "memory");

  if(!initialized){
    if(format == FXSAVE){
      initial.bytes[0] = 0x7f;
      initial.bytes[1] = 0x03;
      initial.bytes[24] = 0x80;
      initial.bytes[25] = 0x1f;
    } else if(format == X87){
      asm volatile("fninit; fldz; fldz; fldz; fldz; fldz; fldz; fldz; fldz; fninit;"
                   "fnsave %0" : "=m"(initial));
    }
    initialized = 1;

  }
  fpureset();
}

const char *
fpumode(void)
{
  return format == XSAVE ? "AVX XSAVE" : format == FXSAVE ? "SSE FXSAVE" : "x87";
}

void
fpureset(void)
{
  fpurestore(&initial);
}

void
fpusave(void *buffer)
{
  struct state *state = buffer;

  if(format == XSAVE){
    // the header also belongs to this save even when state is in its initial form
    memset(state->bytes + 512, 0, 64);
    asm volatile("xsave %0" : "+m"(*state) : "a"(7), "d"(0));
  } else if(format == FXSAVE)
    asm volatile("fxsave %0" : "=m"(*state));
  else
    asm volatile("fnsave %0; frstor %0" : "=m"(*state));
}

void
fpurestore(const void *buffer)
{
  const struct state *state = buffer;

  if(format == XSAVE)
    asm volatile("xrstor %0" : : "m"(*state), "a"(7), "d"(0));
  else if(format == FXSAVE)
    asm volatile("fxrstor %0" : : "m"(*state));
  else
    asm volatile("fnclex; frstor %0" : : "m"(*state));
}
