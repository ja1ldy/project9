/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "loader.h"
#include "bootargs.h"

int
main(void)
{
  const struct boot_args *args = (const struct boot_args*)__args;
  uint entry;
  enum boot_action action;

  serial_init();
  action = menu(args->bios_disk);
  text_mode();
  if(action == BOOT_X86_64 || action == BOOT_X86_64_LIVE){
    int live64 = action == BOOT_X86_64_LIVE;
    if(!long_mode_available()){
      puts_screen("x86_64 long mode unavailable, booting i386\n");
      puts_serial("x86_64 long mode unavailable, booting i386\n");
    } else {
      entry = load_kernel64(args->bios_disk);
      if(entry && boot_hardware() == 0 &&
         boot_root(args->bios_disk, live64) == 0){
        puts_screen("Loading Project9 x86_64\n");
        puts_serial("Loading Project9 x86_64\n");
        __exec(__base + (uint)kernel_jump, entry);
      }
      puts_screen("x86_64 boot payload unavailable or invalid, booting i386\n");
      puts_serial("x86_64 boot payload unavailable or invalid, booting i386\n");
    }
    action = live64 ? BOOT_LIVE : BOOT_DISK;
  }
  puts_screen("Loading Project9 x86\n");
  puts_serial("Loading Project9 x86\n");
  entry = load_kernel(args->bios_disk);
  if(entry){
    if(boot_hardware() == 0){
      if(boot_root(args->bios_disk, action == BOOT_LIVE) == 0)
        __exec(__base + (uint)kernel_jump, entry);
      puts_screen("Cannot load the Project9 RAM root\n");
      puts_serial("Cannot load the Project9 RAM root\n");
    } else {
      puts_screen("Project9 needs at least 8 MiB of contiguous extended memory\n");
      puts_serial("Project9 needs at least 8 MiB of contiguous extended memory\n");
    }
  } else {
    puts_screen("Cannot load the Project9 kernel\n");
    puts_serial("Cannot load the Project9 kernel\n");
  }
  for(;;)
    ;
}

void
exit(void)
{
  __exit();
}
