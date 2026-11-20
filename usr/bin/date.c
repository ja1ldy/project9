/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "date.h"
#include "fcntl.h"

static void
two(uint value)
{
  printf(1, "%c%c", '0' + value / 10, '0' + value % 10);
}

int
main(int argc, char **argv)
{
  struct rtcdate now;
  int fd;

  if(argc != 1){
    printf(2, "usage: date\n");
    return 2;
  }
  fd = open("/dev/rtc", O_RDONLY);
  if(fd < 0 || read(fd, &now, sizeof(now)) != sizeof(now)){
    printf(2, "date: cannot read the hardware clock\n");
    if(fd >= 0)
      close(fd);
    return 1;
  }
  close(fd);
  printf(1, "%u-", now.year);
  two(now.month);
  printf(1, "-");
  two(now.day);
  printf(1, " ");
  two(now.hour);
  printf(1, ":");
  two(now.minute);
  printf(1, ":");
  two(now.second);
  printf(1, "\n");
  return 0;
}
