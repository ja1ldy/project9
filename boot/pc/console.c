/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "loader.h"
#include "version.h"
#include "x86.h"

#define COM1 0x3f8

#define AUTOBOOT_TICKS 54
#define KEY_UP 0x100
#define KEY_DOWN 0x101

static const char *const items[] = {
  [BOOT_DISK] = "1. x86 (32-bit) - Disk",
  [BOOT_LIVE] = "2. x86 (32-bit) - Live",
  [BOOT_X86_64] = "3. x86_64 (64-bit) - Disk",
  [BOOT_X86_64_LIVE] = "4. x86_64 (64-bit) - Live"
};

void *
physical(uint address)
{
  return (void*)(address - __base);
}

void
bios(uint interrupt, uint ax, uint bx, uint dx)
{
  __v86.ctl = BIOS_FLAGS;
  __v86.addr = interrupt;
  __v86.eax = ax;
  __v86.ebx = bx;
  __v86.ecx = 0;
  __v86.edx = dx;
  __v86.efl = 0x202;
  __v86.es = __v86.ds = __v86.fs = __v86.gs = 0;
  __v86.ebp = __v86.esi = __v86.edi = 0;
}

void
serial_init(void)
{
  outb(COM1+1, 0);
  outb(COM1+3, 0x80);
  // match the kernel baud rate to keep the same serial session
  outb(COM1, 12);
  outb(COM1+1, 0);
  outb(COM1+3, 3);
  outb(COM1+2, 1);
  outb(COM1+4, 3);
}

static void
serial_char(int c)
{
  int i;

  // keep boot moving even if there is no serial port
  for(i = 0; i < 20000; i++)
    if(inb(COM1+5) & 0x20){
      outb(COM1, c);
      return;
    }
}

void
puts_serial(const char *s)
{
  while(*s){
    if(*s == '\n')
      serial_char('\r');
    serial_char(*s++);
  }
}

void
puts_screen(const char *s)
{
  while(*s){
    if(*s == '\n'){
      bios(0x10, 0x0e0d, 7, 0);
      __v86int();
    }
    bios(0x10, 0x0e00 | (uchar)*s++, 7, 0);
    __v86int();
  }
}

void
text_mode(void)
{
  bios(0x10, 3, 0, 0);
  __v86int();
}

static void
cursor(uint row, uint col)
{
  bios(0x10, 0x0200, 0, (row << 8) | col);
  __v86int();
}

static int
getkey(void)
{
  int status = inb(COM1+5);

  if(status != 0xff && (status & 1))
    return inb(COM1);
  bios(0x16, 0x0100, 0, 0);
  __v86int();
  if(__v86.efl & BIOS_ZERO)
    return -1;
  bios(0x16, 0, 0, 0);
  __v86int();
  if((__v86.eax & 0xff) != 0 && (__v86.eax & 0xff) != 0xe0)
    return __v86.eax & 0xff;
  switch((__v86.eax >> 8) & 0xff){
  case 0x48: return KEY_UP;
  case 0x50: return KEY_DOWN;
  }
  return -1;
}

static uint
elapsed(uint start, uint now)
{
  if(now >= start)
    return now - start;
  return 0x1800b0 - start + now;
}

static void
selection(int graphics, int count, int selected)
{
  int i;

  if(graphics){
    boot_video_selection(items, count, selected);
    return;
  }
  for(i = 0; i < count; i++){
    cursor(4+i, 4);
    puts_screen(i == selected ? "> " : "  ");
    puts_screen(items[i]);
  }
}

static int
show_menu(int count, int selected)
{
  if(boot_video_menu(items, count, selected))
    return 1;

  text_mode();
  puts_screen("  Project9\n\n"
              "  +-------- Welcome to Project9 --------+\n");
  selection(0, count, selected);
  cursor(9, 4);
  puts_screen("Enter: boot   Space: pause\n"
              "    Esc: loader prompt   R: reboot\n");
  return 0;
}

static void
status(int graphics, const char *message)
{
  if(graphics){
    boot_video_status(message);
    return;
  }
  cursor(14, 2);
  puts_screen("                                            ");
  cursor(14, 2);
  puts_screen(message);
}

static void
say(const char *message)
{
  puts_screen(message);
  puts_serial(message);
}

static int
equal(const char *a, const char *b)
{
  while(*a && *a == *b){
    a++;
    b++;
  }
  return *a == *b;
}

static int
loader_prompt(void)
{
  char line[32], echo[2] = {0, 0};
  uint len;
  int key;

  text_mode();
  say("Project9 loader\n"
      "Type help for commands\n");
  for(;;){
    say("loader> ");
    len = 0;
    for(;;){
      key = getkey();
      if(key == 0x1b && !len)
        return 0;
      if(key == '\r' || key == '\n')
        break;
      if((key == 8 || key == 0x7f) && len){
        len--;
        say("\b \b");
      } else if(key >= ' ' && key <= '~' && len < sizeof(line)-1){
        line[len++] = key;
        echo[0] = key;
        say(echo);
      }
    }
    line[len] = 0;
    say("\n");
    if(equal(line, "boot"))
      return BOOT_DISK + 1;
    if(equal(line, "live"))
      return BOOT_LIVE + 1;
    if(equal(line, "boot64"))
      return BOOT_X86_64 + 1;
    if(equal(line, "live64"))
      return BOOT_X86_64_LIVE + 1;
    if(equal(line, "menu"))
      return 0;
    if(equal(line, "reboot"))
      __exit();
    if(equal(line, "help"))
      say("boot     Start x86 from disk\n"
          "live     Start x86 with a RAM root\n"
          "boot64   Start Project9 x86_64\n"
          "live64   Start x86_64 with a RAM root\n"
          "menu     Return to the menu\n"
          "reboot   Restart the machine\n");
    else if(len)
      say("Unknown command\n");
  }
}

static void
serial_menu(int count, int selected, int paused)
{
  puts_serial(PROJECT9_RELEASE " boot menu\n");
  for(int i = 0; i < count; i++){
    puts_serial(i == selected ? "> " : "  ");
    puts_serial(items[i]);
    puts_serial("\n");
  }
  puts_serial("Enter: boot  Space: pause  Esc: prompt  R: reboot\n");
  puts_serial(paused ? "Autoboot paused\n" : "Autoboot in 3 seconds\n");
}

enum boot_action
menu(uint disk)
{
  const volatile uint *clock = physical(0x46c);
  static const char *const countdown[] = {
    "Autoboot in 1 second",
    "Autoboot in 2 seconds",
    "Autoboot in 3 seconds"
  };
  uint start, passed;
  int wide = boot_has_kernel64(disk);
  int count = wide ? BOOT_X86_64_LIVE + 1 : BOOT_LIVE + 1;
  enum boot_action default_action = wide ? BOOT_X86_64 : BOOT_DISK;
  int key, paused = 0, selected = default_action, graphics, seconds, shown = 0;

  graphics = show_menu(count, selected);
  serial_menu(count, selected, 0);

  start = *clock;
  for(;;){
    key = getkey();
    if(key == '\r' || key == '\n')
      key = '1' + selected;
    if(key == KEY_UP || key == KEY_DOWN){
      selected += key == KEY_UP ? count-1 : 1;
      selected %= count;
      selection(graphics, count, selected);
      if(!paused)
        key = ' ';
    }

    if(key >= '1' && key < '1' + count)
      return key - '1';
    if(key == 'r' || key == 'R')
      __exit();
    if(key == 0x1b){
      int action = loader_prompt();
      if(action)
        return action - 1;
      selected = default_action;
      graphics = show_menu(count, selected);
      paused = 1;
      status(graphics, "Autoboot paused");
      serial_menu(count, selected, 1);
    } else if(key == ' '){
      paused = !paused;
      if(paused){
        status(graphics, "Autoboot paused");
        puts_serial("Autoboot paused\n");
      } else {
        selected = default_action;
        selection(graphics, count, selected);
        start = *clock;
        shown = 0;
        puts_serial("Autoboot resumed\n");
      }
    }

    if(!paused){
      passed = elapsed(start, *clock);
      if(passed >= AUTOBOOT_TICKS)
        return default_action;
      seconds = 3 - passed / 18;
      if(seconds != shown){
        status(graphics, countdown[seconds-1]);
        shown = seconds;
      }
    }
  }
}
