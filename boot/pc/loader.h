/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */
#ifndef PROJECT9_BOOT_LOADER_H
#define PROJECT9_BOOT_LOADER_H

#include "types.h"

#define BIOS_FLAGS 0x40000
#define BIOS_CARRY 0x0001
#define BIOS_ZERO  0x0040
#define BOOT_MENU_COUNT 7

enum boot_action { BOOT_DISK, BOOT_LIVE, BOOT_X86_64, BOOT_X86_64_LIVE };

struct bios_regs {
  uint ctl, addr, es, ds, fs, gs;
  uint eax, ecx, edx, ebx, efl, ebp, esi, edi;
};

extern struct bios_regs __v86;
extern uint __base, __args;

void __v86int(void);
void __exit(void) __attribute__((noreturn));
void __exec(uint, uint) __attribute__((noreturn));
void kernel_jump(void);
void *physical(uint);
void bios(uint, uint, uint, uint);
void serial_init(void);
void puts_serial(const char *);
void puts_screen(const char *);
void text_mode(void);
int menu(void);
int boot_video_menu(const char *const *, int);
void boot_video_selection(const char *const *, int);
void boot_video_status(const char *);
uint load_kernel(uint);
uint load_kernel64(uint);
int long_mode_available(void);
int boot_root(uint, int);
int boot_hardware(void);

#endif
