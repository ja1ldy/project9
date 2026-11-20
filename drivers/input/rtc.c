/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "date.h"
#include "x86.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "p9abi.h"

#define CMOS_SELECT 0x70
#define CMOS_DATA 0x71
#define CMOS_STATUS_A 0x0a
#define CMOS_STATUS_B 0x0b
#define CMOS_UPDATING 0x80
#define CMOS_CENTURY 0x32

static struct spinlock rtclock;
static uint epoch;

static uint
reg(uint index)
{
  outb(CMOS_SELECT, index);
  return inb(CMOS_DATA);
}

static void
sample(struct rtcdate *r)
{
  r->second = reg(0);
  r->minute = reg(2);
  r->hour = reg(4);
  r->day = reg(7);
  r->month = reg(8);
  r->year = reg(9);
}

static uint
decimal(uint n)
{
  return (n >> 4) * 10 + (n & 15);
}

static int
time_read(struct rtcdate *out)
{
  struct rtcdate a, b;
  uint flags, century, pm;
  int i;

  acquire(&rtclock);
  for(i = 0; i < 1000; i++){
    if(reg(CMOS_STATUS_A) & CMOS_UPDATING)
      continue;
    flags = reg(CMOS_STATUS_B);
    sample(&a);
    century = reg(CMOS_CENTURY);
    sample(&b);
    if(!(reg(CMOS_STATUS_A) & CMOS_UPDATING) && !memcmp(&a, &b, sizeof(a)))
      break;
  }
  release(&rtclock);
  if(i == 1000)
    return -1;

  pm = a.hour & 0x80;
  a.hour &= 0x7f;
  if(!(flags & 4)){
    a.second = decimal(a.second);
    a.minute = decimal(a.minute);
    a.hour = decimal(a.hour);
    a.day = decimal(a.day);
    a.month = decimal(a.month);
    a.year = decimal(a.year);
    century = decimal(century);
  }
  if(!(flags & 2))
    a.hour = a.hour % 12 + (pm ? 12 : 0);
  if(a.second > 59 || a.minute > 59 || a.hour > 23 || a.day < 1 ||
     a.day > 31 || a.month < 1 || a.month > 12 || a.year > 99)
    return -1;

  // use the pc century register when the firmware keeps it valid
  if(century >= 19 && century <= 29)
    a.year += century * 100;
  else
    a.year += a.year >= 70 ? 1900 : 2000;
  *out = a;
  return 0;
}

static int
rtc_read(struct inode *ip, char *dst, int n, uint off, int nonblock)
{
  struct rtcdate now;

  if(off || !n)
    return 0;
  if(ip->minor != 0 || n < sizeof(now) || time_read(&now) < 0)
    return -1;
  memmove(dst, &now, sizeof(now));
  return sizeof(now);
}

static int
leap(uint year)
{
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static uint
epochseconds(struct rtcdate *r)
{
  static const uint months[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  uint year, i, days = 0, extra;

  if(r->year < 1970 || r->year > 2038)
    return 0;
  for(year = 1970; year < r->year; year++)
    days += 365 + leap(year);
  for(i = 1; i < r->month; i++)
    days += months[i-1] + (i == 2 && leap(year));
  extra = r->month == 2 && leap(year);
  if(r->day > months[r->month-1] + extra)
    return 0;
  days += r->day - 1;
  return days * 86400 + r->hour * 3600 + r->minute * 60 + r->second;
}

int
rtcgettime(struct timeval *out)
{
  uint elapsed;

  acquire(&tickslock);
  elapsed = ticks;
  release(&tickslock);
  if(!epoch || epoch > 0x7fffffffU - elapsed / P9_HZ)
    return -1;
  out->tv_sec = epoch + elapsed / P9_HZ;
  out->tv_usec = (elapsed % P9_HZ) * (1000000 / P9_HZ);
  return 0;
}

void
rtcinit(void)
{
  struct rtcdate now;

  initlock(&rtclock, "rtc");
  if(time_read(&now) == 0)
    epoch = epochseconds(&now);
  devsw[DEV_RTC].read = rtc_read;
}
