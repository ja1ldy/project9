/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <project9/cpufeatures.h>
#include <string.h>

static size_t
copy_avx(unsigned char *dst, const unsigned char *src, size_t n)
{
  size_t done = 0;

  for(; n - done >= 32; done += 32)
    asm volatile("vmovdqu (%0), %%ymm0; vmovdqu %%ymm0, (%1)"
                 : : "r"(src + done), "r"(dst + done) : "memory");
  asm volatile("vzeroupper" : : : "memory");
  return done;
}

static size_t
copy_sse(unsigned char *dst, const unsigned char *src, size_t n)
{
  size_t done = 0;

  for(; n - done >= 16; done += 16)
    asm volatile("movups (%0), %%xmm0; movups %%xmm0, (%1)"
                 : : "r"(src + done), "r"(dst + done) : "memory");
  return done;
}

enum copy_method { detect, integer, sse, avx };

static enum copy_method
detect_method(void)
{
  unsigned int flags[4], xcr0, high;

  if(!x86_has_cpuid())
    return integer;
  x86_cpuid(0, 0, flags);
  if(flags[0] < 1)
    return integer;
  x86_cpuid(1, 0, flags);
  if((flags[2] & (X86_AVX | X86_OSXSAVE)) == (X86_AVX | X86_OSXSAVE)){
    asm volatile("xgetbv" : "=a"(xcr0), "=d"(high) : "c"(0));
    if((xcr0 & 6) == 6)
      return avx;
  }
  return (flags[3] & X86_SSE) ? sse : integer;
}

void *
memcpy(void *output, const void *input, size_t n)
{
  static int selected;
  unsigned char *dst = output;
  const unsigned char *src = input;
  size_t done = 0;

  if(n >= 256){
    int method = __atomic_load_n(&selected, __ATOMIC_RELAXED);
    if(method == detect){
      method = detect_method();
      __atomic_store_n(&selected, method, __ATOMIC_RELAXED);
    }
    if(method == avx)
      done = copy_avx(dst, src, n);
    else if(method == sse)
      done = copy_sse(dst, src, n);
  }
  dst += done;
  src += done;
  n -= done;
  size_t words = n / 4;
  n %= 4;
  asm volatile("cld; rep movsl" : "+D"(dst), "+S"(src), "+c"(words) : : "memory");
  asm volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
  return output;
}
