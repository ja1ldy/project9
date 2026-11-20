/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "p9abi.h"
#include "pci.h"
#include "reboot.h"
#include "x86.h"

int
sys_reboot(void)
{
  int how, i, result;

  if(myproc()->uid)
    return -P9_EPERM;
  if(argint(0, &how) < 0 || (how != RB_POWEROFF && how != RB_REBOOT))
    return -P9_EINVAL;
  result = fsfreeze();
  if(result < 0)
    return result;
  cprintf("Project9: filesystems synchronized\n");
  cli();
  if(how == RB_POWEROFF){
    acpipoweroff();
    if(pci_present(0x11111234)){
      outw(0x604, 0x2000);
      outw(0xb004, 0x2000);
    }
    cprintf("Project9: halted, power can be switched off\n");
  } else {
    acpireset();
    outb(0xcf9, 0x02);
    outb(0xcf9, 0x06);
    for(i = 0; i < 100000 && (inb(0x64) & 2); i++)
      ;
    if(i < 100000)
      outb(0x64, 0xfe);
    cprintf("Project9: reset unavailable, halted\n");
  }
  for(;;)
    asm volatile("hlt");
}
