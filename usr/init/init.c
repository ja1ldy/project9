/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "tty.h"
#include "devices.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid;

  mkdir("/dev");
  if(open("/dev/console", O_RDWR) < 0){
    mknod("/dev/console", 1, 1);
    open("/dev/console", O_RDWR);
  }
  dup(0);
  dup(0);

  mknod("/dev/keyboard", DEV_KEYBOARD, 0);
  mknod("/dev/mouse", DEV_POINTER, POINTER_MOUSE);
  mknod("/dev/touchpad", DEV_POINTER, POINTER_TOUCHPAD);
  mknod("/dev/fb0", DEV_FB, 0);
  for(int i = 0; i < P9_NPTY; i++){
    char master[] = "/dev/ptyp0", slave[] = "/dev/ttyp0";
    master[9] = slave[9] = '0' + i;
    mknod(master, DEV_PTY_MASTER, i);
    mknod(slave, DEV_PTY_SLAVE, i);
  }
  mknod("/dev/null", DEV_MEMORY, MEMORY_NULL);
  mknod("/dev/zero", DEV_MEMORY, MEMORY_ZERO);
  mknod("/dev/rtc", DEV_RTC, 0);

  if(usbquery(0) > 0){
    char path[] = "/dev/usb0";
    for(int i = 0; i < USB_DISK_COUNT; i++){
      path[8] = '0'+i;
      mknod(path, DEV_USB, i);
      chmod(path, 0600);
    }
    if(fork() == 0){
      // keep automatic enumeration in process context
      for(;;){
        sleep(100);
        usbquery(0);
      }
    }
  }

  for(;;){
    printf(1, "init: starting /bin/login\n");
    pid = forkjob(JOB_SESSION);
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){
      exec("/bin/login", argv);
      printf(1, "init: exec /bin/login failed\n");
      exit();
    }
    while(jobctl(pid, JOB_FOREGROUND) == JOB_STOPPED)
      jobctl(pid, JOB_RESUME);
  }
}
