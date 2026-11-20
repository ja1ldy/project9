/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "loader.h"
#include "boothw.h"
#include "memlayout.h"
#include "x86.h"

#define BIOS_BUFFER 0x8000
#define SMAP 0x534d4150U

struct e820 {
  uint base, basehi, size, sizehi, type, attr;
};

static uint
memory_end(void)
{
  struct e820 *info = physical(BIOS_BUFFER);
  uint next = 0, end, i;

  // use the contiguous usable region that holds the kernel
  for(i = 0; i < 128; i++){
    bios(0x15, 0xe820, next, SMAP);
    __v86.ecx = sizeof(*info);
    __v86.edi = BIOS_BUFFER;
    info->attr = 1;
    __v86int();
    if((__v86.efl & BIOS_CARRY) || __v86.eax != SMAP || __v86.ecx < 20)
      break;
    if(info->type == 1 && !info->basehi && info->base <= EXTMEM &&
       (__v86.ecx < sizeof(*info) || (info->attr & 1))){
      end = info->sizehi || info->size > ~0U - info->base ?
            ~0U : info->base + info->size;
      if(end > EXTMEM)
        return end;
    }
    if(!__v86.ebx || next == __v86.ebx)
      break;
    next = __v86.ebx;
  }

  // older bioses can report the contiguous extended memory directly
  bios(0x15, 0x8800, 0, 0);
  __v86int();
  return (__v86.efl & BIOS_CARRY) ? 0 : EXTMEM + (__v86.eax & 0xffff) * 1024;
}

static uint
word(const uchar *p)
{
  return p[0] | ((uint)p[1] << 8);
}

static uint
dword(const uchar *p)
{
  return word(p) | (word(p+2) << 16);
}

static void
video_mode(struct boothw *hw)
{
  uchar *info = physical(BIOS_BUFFER);
  uint modes, mode, best = 0, best_area = 0, i;
  uint width, height, stride, fbaddr, format, version;

  stosb(info, 0, 512);
  info[0] = 'V'; info[1] = 'B'; info[2] = 'E'; info[3] = '2';
  bios(0x10, 0x4f00, 0, 0);
  __v86.edi = BIOS_BUFFER;
  __v86int();
  if((__v86.eax & 0xffff) != 0x004f || dword(info) != 0x41534556)
    return;
  version = word(info+4);
  modes = (word(info+16) << 4) + word(info+14);
  if(version < 0x0200 || modes < 0x400 || modes >= 0x100000)
    return;

  // keep the controller block intact in case the mode list lives inside it
  info = physical(BIOS_BUFFER + 512);
  for(i = 0; i < 256 && modes < 0xfffff; i++, modes += 2){
    mode = word(physical(modes));
    if(mode == 0xffff)
      break;
    bios(0x10, 0x4f01, 0, 0);
    __v86.ecx = mode;
    __v86.edi = BIOS_BUFFER + 512;
    __v86int();
    if((__v86.eax & 0xffff) != 0x004f || (word(info) & 0x91) != 0x91 ||
       info[25] != 32 || info[27] != 6)
      continue;
    width = word(info+18);
    height = word(info+20);
    fbaddr = dword(info+40);
    stride = version >= 0x0300 ? word(info+50) : 0;
    format = stride ? 54 : 31;
    if(!stride)
      stride = word(info+16);
    if(width < 640 || height < 480 || width > BOOT_FB_MAX_WIDTH ||
       height > BOOT_FB_MAX_HEIGHT || stride < width * 4 || (stride & 3) ||
       stride > BOOT_FB_MAX_STRIDE || fbaddr < PHYSTOP || (fbaddr & 4095) ||
       stride * height > ~0U - fbaddr || width * height <= best_area)
      continue;
    if(info[format] != 8 || info[format+1] != 16 ||
       info[format+2] != 8 || info[format+3] != 8 ||
       info[format+4] != 8 || info[format+5] != 0)
      continue;
    best = mode;
    best_area = width * height;
    hw->fbaddr = fbaddr;
    hw->width = width;
    hw->height = height;
    hw->stride = stride;
  }
  if(!best)
    return;
  bios(0x10, 0x4f02, best | 0x4000, 0);
  __v86int();
  if((__v86.eax & 0xffff) != 0x004f){
    hw->fbaddr = 0;
    text_mode();
  }
}

int
boot_hardware(void)
{
  struct boothw *hw = physical(BOOTHW_ADDR);

  stosb(hw, 0, sizeof(*hw));
  hw->memend = memory_end();
  if(hw->memend < 8 * 1024 * 1024)
    return -1;
  video_mode(hw);
  hw->magic = BOOTHW_MAGIC;
  return 0;
}

int
long_mode_available(void)
{
  uint original, changed, a, b, c, d;

  // older x86 processors cannot toggle the cpuid flag
  asm volatile("pushfl; popl %0" : "=r"(original));
  changed = original ^ (1U << 21);
  asm volatile("pushl %0; popfl" : : "r"(changed) : "cc", "memory");
  asm volatile("pushfl; popl %0" : "=r"(changed));
  asm volatile("pushl %0; popfl" : : "r"(original) : "cc", "memory");
  if(!((original ^ changed) & (1U << 21)))
    return 0;
  asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(0x80000000U));
  if(a < 0x80000001U)
    return 0;
  asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(0x80000001U));
  return !!(d & (1U << 29));
}
