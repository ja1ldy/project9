/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "stat.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "fs.h"
#ifdef P9_LEGACY_ROOT
#include "legacyfs.h"
#endif
#include "wait.h"
#include "p9abi.h"
#include "signal.h"
#include "syscall.h"
#include "traps.h"
#include "filemap.h"
#include "elf.h"
#include "diskuser.h"
#include "local.h"
#include "ioctl.h"
#include "termios.h"
#include "tty.h"

int mmap(void *, int, int, int, int, int);
int munmap(void *, int);

static void
require(int ok, const char *message)
{
  if(!ok){
    printf(2, "stability: %s\n", message);
    exitstatus(1);
  }
}

static void
bad_arguments(void)
{
  int fd, channel[2];
  char *brk = sbrk(0);
  char *guard = brk - (USTACKPAGES + 1) * PGSIZE;
  char byte = 'a', actual = 0;
  char *outside = (char*)KERNBASE;
  char *empty[] = {0};
  struct stat st;

  fd = open("/dev/zero", O_RDONLY);
  require(fd >= 0, "cannot open /dev/zero");
  require(read(fd, 0, 1) == -1 && read(fd, outside, 1) == -1,
          "read accepted an invalid destination");
  require(read(fd, brk - 1, 2) == -1 && read(fd, &byte, -1) == -1,
          "read accepted an invalid length or range");
  require(read(fd, guard, 1) == -1 && read(fd, guard - 1, 2) == -1 &&
          open(guard, O_RDONLY) == -1,
          "the kernel accepted a buffer in the stack guard page");
  require(read(fd, outside, 0) == 0 && byte == 'a',
          "zero length read changed the buffer or failed");
  require(fstat(fd, (struct stat*)outside) == -1 && fstat(fd, &st) == 0,
          "fstat failed to distinguish valid and invalid destinations");
  require(open(outside, O_RDONLY) == -1 &&
          exec("/bin/true", (char**)outside) == -1 &&
          exec(outside, empty) == -1,
          "the kernel accepted an invalid path or argument vector");
  require(pipe((int*)outside) == -1 && pipe(channel) == 0,
          "pipe accepted an invalid descriptor buffer");
  require(read(channel[0], outside, 0) == 0, "empty pipe blocked a zero length read");
  require(write(channel[1], outside, 1) == -1 &&
          write(channel[1], &byte, 1) == 1 &&
          read(channel[0], &actual, 1) == 1 && actual == byte,
          "pipe data changed after an invalid write");
  close(channel[0]);
  close(channel[1]);
  close(fd);
  printf(1, "stability: invalid syscall arguments rejected\n");
}

static void
memory_pressure(void)
{
  char *brk = sbrk(0);
  int before = freemem();

  require(sbrk(-1) == (char*)-1 &&
          sbrk(-2147483647 - 1) == (char*)-1 &&
          sbrk(2147483647) == (char*)-1 && sbrk(0) == brk,
          "sbrk moved past the address space limits");
  require(sbrk(before + PGSIZE) == (char*)-1 && sbrk(0) == brk,
          "an exhausted allocation changed the break");
  require(freemem() == before, "a failed allocation left pages behind");
  require(sbrk(2 * PGSIZE) == brk && brk[0] == 0 && brk[PGSIZE] == 0,
          "memory was unavailable or dirty after rollback");
  brk[0] = 42;
  require(sbrk(-2 * PGSIZE) == brk + 2 * PGSIZE && freemem() == before,
          "shrinking the heap did not return its pages");
  printf(1, "stability: memory exhaustion recovered\n");
}

static int
round_trip(int code)
{
  int pid = fork();
  int status;

  if(pid == 0)
    exitstatus(code);
  if(pid < 0)
    return 0;
  return waitpid(pid, &status, 0) == pid &&
         WIFEXITED(status) && WEXITSTATUS(status) == code;
}

static void
process_pressure(void)
{
  int i, pid, channel[2], kids[NPROC];
  int count = 0, ok = 1, before = freemem();
  char byte;

  for(i = 0; i < 3 * NPROC; i++)
    require(round_trip(i & 0xff), "fork and wait stopped working");
  require(pipe(channel) == 0, "cannot open the process test pipe");
  for(i = 0; i < NPROC; i++){
    pid = fork();
    if(pid == 0){
      close(channel[1]);
      exitstatus(read(channel[0], &byte, 1) == 0 ? 0 : 1);
    }
    if(pid < 0)
      break;
    kids[count++] = pid;
  }
  close(channel[0]);
  close(channel[1]);
  for(i = 0; i < count; i++){
    int status;
    if(waitpid(kids[i], &status, 0) != kids[i] ||
       !WIFEXITED(status) || WEXITSTATUS(status) != 0)
      ok = 0;
  }
  require(count > 0 && count < NPROC && ok && round_trip(42),
          "process exhaustion did not recover");
  require(freemem() == before, "process creation left pages behind");
  printf(1, "stability: repeated fork and process exhaustion recovered\n");
}

static void
hardening(void)
{
  static char arg[PGSIZE];
  uint seed = 0x54d9a230;
  uptr flags;
  int fd = open("/dev/zero", O_RDONLY), status, pid, self = getpid();
  char *ro, *rw;
  char *args[] = { "true", arg, arg, 0 };

  require(fd >= 0, "cannot open /dev/zero");
  for(int i = 0; i < 256; i++){
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    require(read(fd, (void*)(uptr)(KERNBASE | seed), (seed & 255)+1) < 0,
            "read accepted a kernel address");
    require(mmap(0, FILEMAP_LIMIT + (seed & 4095)+1, PROT_READ,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) < 0,
            "mmap accepted an excessive length");
  }
  require(sigreturn() < 0, "sigreturn accepted an absent signal frame");
  require(waitpid(-2147483647-1, &status, WNOHANG) < 0 &&
          send_signal(-2147483647-1, 0) < 0, "invalid process selector accepted");

  memset(arg, 'x', sizeof(arg));
  arg[sizeof(arg)/2+8] = 0;
  require(execve("/bin/true", args, 0) < 0, "exec accepted an oversized argument vector");
  struct elfhdr elf = { .magic = ELF_MAGIC };
  require(chdir("/tmp") == 0, "cannot enter /tmp");
  int image = open("badelf.tmp", O_WRONLY | O_CREATE | O_EXCL);
  require(image >= 0, "cannot create malformed elf test file");
  int written = write(image, &elf, sizeof(elf));
  close(image);
  char *elfargs[] = { "badelf.tmp", 0 };
  int rejected = written == sizeof(elf) && chmod("badelf.tmp", 0700) == 0 &&
                 execve("badelf.tmp", elfargs, 0) < 0;
  require(unlink("badelf.tmp") == 0 && rejected, "exec accepted an incomplete elf header");

  ro = (char*)(uptr)mmap(0, PGSIZE, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  rw = (char*)(uptr)mmap(0, PGSIZE+1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  require((uptr)ro < KERNBASE && (uptr)rw < KERNBASE && ro && rw, "cannot map private test pages");
  require(read(fd, ro, 1) < 0 && read(fd, rw+PGSIZE, 2) < 0 &&
          read(fd, rw+PGSIZE, 1) == 1 && munmap(rw+1, PGSIZE) < 0,
          "a syscall crossed the mapping boundary or ignored permissions");
  rw[0] = 'p';
  pid = fork();
  require(pid >= 0, "cannot fork mapping test");
  if(!pid){
    rw[0] = 'c';
    exitstatus(rw[0] == 'c' && read(fd, ro, 1) < 0 ? 0 : 1);
  }
  require(waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
          !WEXITSTATUS(status) && rw[0] == 'p', "fork did not isolate private pages");
  pid = fork();
  require(pid >= 0, "cannot fork fault test");
  if(!pid){
    *(volatile char*)ro = 'x';
    exitstatus(1);
  }
  require(waitpid(pid, &status, 0) == pid && WIFSIGNALED(status) &&
          WTERMSIG(status) == SIGSEGV, "a write to a read only page did not fault");
  require(munmap(ro, PGSIZE) == 0 && munmap(rw, PGSIZE+1) == 0 &&
          read(fd, rw, 1) < 0, "unmapped pages remained accessible");
  close(fd);

  int gate[2];
  require(pipe(gate) == 0, "cannot open termination test pipe");
  pid = fork();
  require(pid >= 0, "cannot fork termination test");
  if(!pid){
    char byte;
    close(gate[1]);
    read(gate[0], &byte, 1);
    exitstatus(1);
  }
  close(gate[0]);
  require(kill(pid) == 0 && waitpid(pid, &status, 0) == pid &&
          WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL,
          "killed child did not report signal termination");
  close(gate[1]);

  if(getuid() == 0){
    pid = fork();
    require(pid >= 0, "cannot fork credential test");
    if(!pid)
      exitstatus(setuid(1200) == 0 && setuid(0) < 0 &&
                 send_signal(self, 0) < 0 ? 0 : 1);
    require(waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
            WEXITSTATUS(status) == 0, "credential isolation failed");
  }

  int result;
  asm volatile("std; int $%c[gate]; pushf; pop %1; cld"
               : "=a"(result), "=r"(flags)
               : "0"(SYS_getpid), [gate] "i"(T_SYSCALL) : "cc", "memory");
  require(result == self && (flags & 0x400), "trap entry did not preserve user flags");
  printf(1, "stability: memory maps exec traps and credential isolation passed\n");
}

static void
smp(void)
{
  enum { workers = 8, rounds = 18 };
  int gate[2], reports[2], kids[workers], status, mask = 0;
  int online = cpuquery(1);
  uint pool = getpid();
  char dir[] = "/tmp/smp.00000000";
  const char *hex = "0123456789abcdef";

  require(online >= 2 && online <= NCPU, "smp needs two online guest processors");
  require(cpuquery(2) < 0, "cpuquery accepted an invalid selector");
  for(int i = 0; i < 8; i++, pool >>= 4)
    dir[sizeof(dir)-2-i] = hex[pool & 15];
  require(mkdir(dir) == 0 && chdir(dir) == 0, "cannot create smp test directory");
  require(pipe(gate) == 0 && pipe(reports) == 0, "cannot open smp test pipes");

  for(int i = 0; i < workers; i++){
    kids[i] = fork();
    require(kids[i] >= 0, "cannot start smp worker");
    if(kids[i] == 0){
      uint seed = 0xf114ab28U + i, used = 0;
      char token = 0, filename[] = "data0", alias[] = "link0";
      close(gate[1]);
      close(reports[0]);
      filename[4] = alias[4] = '0'+i;
      require(read(gate[0], &token, 1) == 0, "smp gate did not open");
      close(gate[0]);
      for(int j = 0; j < rounds; j++){
        int cpu = cpuquery(0);
        require(cpu >= 0 && cpu < NCPU, "invalid running cpu index");
        used |= 1U << cpu;
        seed = seed*1664525U + 1013904223U;
        int size = 1 + (seed & (PGSIZE-1));
        char *page = (char*)(uptr)mmap(0, size, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        int fd = open(filename, O_CREATE | O_EXCL | O_RDWR);
        require(page && (uptr)page < KERNBASE && fd >= 0 && page[size-1] == 0,
                "cannot allocate private memory or a file");
        page[size-1] = i+1;
        require(write(fd, page, size) == size && read(fd, page, -1) < 0,
                "parallel write or syscall validation failed");
        require(close(fd) == 0 && link(filename, alias) == 0 &&
                unlink(filename) == 0 && unlink(alias) == 0 &&
                munmap(page, size) == 0, "parallel inode or memory cleanup failed");
        if(!(j % 6)){
          int pid = fork();
          require(pid >= 0, "cannot fork parallel exec test");
          if(!pid){
            char *argv[] = { "true", 0 };
            exec("/bin/true", argv);
            exitstatus(1);
          }
          require(waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
                  WEXITSTATUS(status) == 0, "parallel exec or wait failed");
        }
      }
      require(write(reports[1], &used, sizeof used) == sizeof used,
              "cannot report processor activity");
      exitstatus(0);
    }
  }

  close(gate[0]);
  close(gate[1]);
  close(reports[1]);
  for(int i = 0; i < workers; i++){
    uint used;
    require(waitpid(kids[i], &status, 0) == kids[i] && WIFEXITED(status) &&
            WEXITSTATUS(status) == 0, "parallel worker failed");
    require(read(reports[0], &used, sizeof used) == sizeof used,
            "worker did not report processor activity");
    mask |= used;
  }
  close(reports[0]);
  require(chdir("/") == 0 && unlinkdir(dir) == 0, "cannot clean up smp test directory");
  require(mask && (mask & (mask-1)), "parallel workers only ran on one cpu");
  printf(1, "SMP TESTS PASSED (%d online cpu mask 0x%x)\n", online, mask);
}

static void
storage_guard(const char *partition)
{
  int disk = diskid(partition, 1);
  struct diskinfo info;
  static uchar data[2*DC_SECTOR];

  require(disk >= 0 && diskctl(DC_INFO, disk, 0, 1, &info) == 0 &&
          (info.flags & DC_TEST) && info.first && info.count, "requires a partitioned test disk");
  require(diskctl(DC_WRITE_PART, disk, info.count-1, 2, data) == -P9_EINVAL &&
          diskctl(DC_WRITE_PART, disk, info.count, 1, data) == -P9_EINVAL &&
          diskctl(DC_BOOTBLOCK, disk, info.first-1, 2, data) == -P9_EINVAL,
          "disk controller accepted a transfer across a boundary");
  printf(1, "stability: partition boundaries passed\n");
}

static void
storage_io(const char *operation)
{
  int reading = !strcmp(operation, "read");
  int flushing = !strcmp(operation, "flush"), before_write = 0;
  char byte = 'T';

  if(!strcmp(operation, "metadata")){
    require(mkdir("/root/interrupted-mkdir") < 0,
            "mkdir did not report an interrupted directory update");
  } else {
    require(reading || flushing || !strcmp(operation, "write"),
            "choose read write flush or metadata for the storage test");
    int fd = open("/root/reliability", reading ? O_RDONLY : O_WRONLY);
    require(fd >= 0, "cannot open the storage fault test file");
    if(reading){
      require(read(fd, &byte, 1) < 0, "read did not report the injected failure");
    } else {
      int n = write(fd, &byte, 1);
      if(flushing){
        // the fault can hit either the dirty marker barrier or sync
        require(n < 0 || n == 1, "unexpected write result with the injected flush failure");
        before_write = n < 0;
      } else {
        require(n < 0, "write did not report the injected failure");
      }
    }
    close(fd);
  }
  require(disksync() == -P9_EIO, "sync did not report the storage failure");
  require(open("/root/after-failure", O_CREATE | O_WRONLY) < 0,
          "faulted filesystem accepted new writes");
  printf(1, "stability: storage %s failure reported and writes blocked%s\n",
         operation, flushing ? (before_write ? " (dirty marker)" : " (sync)") : "");
}

#ifndef P9_LEGACY_ROOT
static void
inode_pressure(void)
{
  int count, fd;
  char name[] = "i0000";

  require(mkdir("/tmp/inode-pressure") == 0 && chdir("/tmp/inode-pressure") == 0,
          "cannot create inode test directory");
  for(count = 0; count < 2048; count++){
    for(int n = count, i = 4; i > 0; i--, n /= 10)
      name[i] = '0'+n%10;
    fd = open(name, O_CREATE | O_EXCL | O_WRONLY);
    if(fd < 0)
      break;
    close(fd);
  }
  for(int j = 0; j < count; j++){
    for(int n = j, i = 4; i > 0; i--, n /= 10)
      name[i] = '0'+n%10;
    require(unlink(name) == 0, "could not reclaim an inode");
    if(!j){
      fd = open("reuse", O_CREATE | O_EXCL | O_WRONLY);
      require(fd >= 0, "could not reuse a freed inode");
      close(fd);
      require(unlink("reuse") == 0, "could not clean up reused inode");
    }
  }
  require(chdir("/") == 0 && unlinkdir("/tmp/inode-pressure") == 0,
          "could not clean up inode test directory");
  require(count > 0 && count < 2048, "did not exhaust inodes on the test disk");
  printf(1, "stability: inode exhaustion recovered\n");
}
#endif

#ifdef P9_LEGACY_ROOT
static void
fillname(char *name, int i)
{
  strcpy(name, "fill000");
  name[4] += i / 100;
  name[5] += i / 10 % 10;
  name[6] += i % 10;
}

static void
disk_pressure(void)
{
  static char data[3 * BSIZE];
  char name[8];
  struct stat st;
  int fd, n, bytes, request, i, cleanup = 1;
  int count = 0, full = 0, recovered = 0;
  const char *dir = "/tmp/stability";

  require(mkdir(dir) == 0, "cannot create /tmp/stability");
  require(chdir(dir) == 0, "cannot enter /tmp/stability");
  memset(data, 'w', sizeof(data));
  for(i = 0; i < FSSIZE / MAXFILE + 3; i++){
    fillname(name, i);
    fd = open(name, O_WRONLY | O_CREATE | O_EXCL);
    if(fd < 0)
      break;
    count++;
    for(bytes = 0; bytes < MAXFILE * BSIZE; bytes += n){
      request = MAXFILE * BSIZE - bytes;
      if(request > sizeof(data))
        request = sizeof(data);
      n = write(fd, data, request);
      if(n != request)
        break;
    }
    close(fd);
    if(bytes < MAXFILE * BSIZE)
      break;
  }

  fd = open("probe", O_WRONLY | O_CREATE | O_EXCL);
  if(fd >= 0){
    // one block may still be free when an indirect block needs two
    n = write(fd, data, BSIZE);
    full = n == -1 || (n == BSIZE && write(fd, data, BSIZE) == -1);
    close(fd);
  }
  if(full){
    if(mkdir("sub") == 0){
      unlinkdir("sub");
      full = 0;
    } else if(stat("sub", &st) >= 0)
      full = 0;
  }
  if(fd >= 0)
    cleanup = unlink("probe") == 0;
  if(count > 0){
    fillname(name, 0);
    cleanup &= unlink(name) == 0;
    fd = open("retry", O_WRONLY | O_CREATE | O_EXCL);
    if(fd >= 0){
      recovered = write(fd, data, sizeof(data)) == sizeof(data) &&
                  fstat(fd, &st) == 0 && st.size == sizeof(data);
      close(fd);
      cleanup &= unlink("retry") == 0;
    }
  }
  for(i = 1; i < count; i++){
    fillname(name, i);
    cleanup &= unlink(name) == 0;
  }
  cleanup &= chdir("/") == 0 && unlinkdir(dir) == 0;
  require(full && recovered && cleanup, "disk exhaustion or cleanup failed");
  printf(1, "stability: full disk rejected writes and recovered space\n");
}

#else
static void
disk_pressure(void)
{
  static char data[8192];
  int fd, n, full = 0, recovered = 0;
  unsigned int written = 0;
  struct stat st;
  const unsigned int limit = 128U * 1024 * 1024;
  const char *dir = "/tmp/stability";

  require(mkdir(dir) == 0, "cannot create /tmp/stability");
  require(chdir(dir) == 0, "cannot enter /tmp/stability");
  fd = open("fill", O_WRONLY | O_CREATE | O_EXCL);
  if(fd >= 0){
    memset(data, 'w', sizeof(data));
    while(written < limit && (n = write(fd, data, sizeof(data))) > 0)
      written += n;
    close(fd);
  }
  if(written < limit && fd >= 0 && mkdir("sub") < 0)
    full = 1;
  else if(stat("sub", &st) == 0)
    unlinkdir("sub");

  if(fd >= 0 && unlink("fill") == 0){
    fd = open("retry", O_WRONLY | O_CREATE | O_EXCL);
    if(fd >= 0){
      recovered = write(fd, data, sizeof(data)) == sizeof(data) &&
                  fstat(fd, &st) == 0 && st.size == sizeof(data);
      close(fd);
      recovered &= unlink("retry") == 0;
    }
  }
  require(chdir("/") == 0 && unlinkdir(dir) == 0, "disk test cleanup failed");
  require(full && recovered, "disk exhaustion or space recovery failed use a small test image");
  printf(1, "stability: full disk rejected writes and recovered space\n");
}

#endif

int socket(int, int, int);
int bind(int, const void *, unsigned);
int connect(int, const void *, unsigned);
int listen(int, int);
int accept(int, void *, unsigned *);
int shutdown(int, int);
int poll(struct pollfd *, unsigned, int);
int openpty(int *);
int setsid(void);

static void
loop(int domain, const void *addr, uint size)
{
  int server = socket(domain, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
  require(server >= 0 && bind(server, addr, size) == 0 &&
          listen(server, 2) == 0 && accept(server, 0, 0) == -P9_EAGAIN,
          "cannot create a stream listener");
  int another = socket(domain, SOCK_STREAM, 0);
  require(another >= 0 && bind(another, addr, size) == -P9_EADDRINUSE,
          "duplicate bind succeeded");
  close(another);
  int client = socket(domain, SOCK_STREAM, 0);
  require(client >= 0 && connect(client, addr, size) == 0, "cannot connect stream");
  struct pollfd waiting = { server, POLLIN, 0 };
  require(poll(&waiting, 1, 0) == 1 && waiting.revents == POLLIN, "listener is not pollable");
  int peer = accept(server, 0, 0);
  char data = 0;
  int queued;
  require(peer >= 0 && ioctl(peer, FIONREAD, &queued) == 0 && queued == 0 &&
          write(client, "a", 1) == 1 && shutdown(client, SHUT_WR) == 0 &&
          ioctl(peer, FIONREAD, &queued) == 0 && queued == 1 &&
          read(peer, &data, 1) == 1 && data == 'a' &&
          ioctl(peer, FIONREAD, &queued) == 0 && queued == 0 &&
          read(peer, &data, 1) == 0 &&
          write(peer, "b", 1) == 1 && read(client, &data, 1) == 1 && data == 'b',
          "stream transfer or half close failed");
  close(peer);
  waiting = (struct pollfd){ client, POLLIN, 0 };
  require(poll(&waiting, 1, 0) == 1 && (waiting.revents & POLLHUP), "stream hangup not visible");
  close(client);
  close(server);
}

static void
desktop(void)
{
  struct sockaddr_un local = { AF_UNIX, "/tmp/p9-socket-test" };
  loop(AF_UNIX, &local, sizeof(local));
  require(unlink(local.sun_path) == 0, "cannot remove unix socket inode");
  struct sockaddr_in inet = { .sin_family = AF_INET, .sin_port = __builtin_bswap16(17591),
                             .sin_addr.s_addr = __builtin_bswap32(INADDR_LOOPBACK) };
  loop(AF_INET, &inet, sizeof(inet));

  int fds[2], ready[2], status;
  struct winsize size = {33, 111, 666, 429}, result = {0};
  require(openpty(fds) >= 0 && isatty(fds[1]) &&
          ioctl(fds[0], TIOCSWINSZ, &size) == 0 &&
          ioctl(fds[1], TIOCGWINSZ, &result) == 0 && result.ws_col == size.ws_col,
          "pty allocation or window size failed");
  char output[8];
  struct termios attrs;
  require(write(fds[1], "a\nb", 3) == 3 && read(fds[0], output, sizeof(output)) == 4 &&
          (output[4] = 0, strcmp(output, "a\r\nb") == 0), "pty newline output was not translated");
  require(tcgetattr(fds[1], &attrs) == 0, "cannot read pty output settings");
  tcflag_t saved = attrs.c_oflag;
  attrs.c_oflag &= ~OPOST;
  require(tcsetattr(fds[1], TCSANOW, &attrs) == 0 &&
          write(fds[1], "\n", 1) == 1 && read(fds[0], output, sizeof(output)) == 1 && output[0] == '\n',
          "pty raw output was translated");
  attrs.c_oflag = saved;
  require(tcsetattr(fds[1], TCSANOW, &attrs) == 0, "cannot restore pty output settings");
  require(pipe(ready) == 0, "cannot create the pty test pipe");
  int pid = fork();
  require(pid >= 0, "cannot fork the pty child");
  if(!pid){
    close(fds[0]);
    close(ready[0]);
    char buf[8];
    require(setsid() > 0 && ioctl(fds[1], TIOCSCTTY, 0) == 0 &&
            read(fds[1], buf, sizeof(buf)) == 2 && buf[0] == 'a' && buf[1] == '\n',
            "pty canonical line discipline failed");
    require(ttyctl(fds[1], TTY_CBREAK) == 0 && write(ready[1], "r", 1) == 1 &&
            read(fds[1], buf, sizeof(buf)) == 1 && buf[0] == 'q' &&
            ttyctl(fds[1], TTY_CANON) == 0,
            "pty cbreak did not deliver a key without a newline");
    exitstatus(0);
  }
  close(fds[1]);
  close(ready[1]);
  require(write(fds[0], "ab\010\r", 4) == 4, "pty master write failed");
  require(read(fds[0], output, sizeof(output)) == 7 &&
          (output[7] = 0, strcmp(output, "ab\b \b\r\n") == 0), "pty newline echo was not translated");
  char childready;
  require(read(ready[0], &childready, 1) == 1 && childready == 'r' &&
          write(fds[0], "q", 1) == 1, "pty cbreak input failed");
  close(ready[0]);
  require(waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
          WEXITSTATUS(status) == 0, "pty child failed");
  close(fds[0]);

  int len = 32*1024*1024;
  int addr = mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  require(addr > 0, "cannot allocate a 32 mib mapping");
  char *buf = (char*)(uptr)addr;
  buf[0] = 'x';
  buf[len - 1] = 'y';
  require(munmap(buf + PGSIZE, PGSIZE) == 0 &&
          buf[0] == 'x' && buf[len - 1] == 'y' &&
          munmap(buf, PGSIZE) == 0 &&
          munmap(buf + PGSIZE * 2, len - PGSIZE * 2) == 0,
          "partial munmap damaged adjacent pages");

  const char *path = "/tmp/p9-large-file-test";
  int file = open(path, O_CREATE | O_EXCL | O_RDWR);
  require(file >= 0, "cannot create large file");
  int offsets[] = { 80*1024, 30*1024*1024, 70*1024*1024 };
  char data = 0;
  for(int i = 0; i < sizeof(offsets)/sizeof(*offsets); i++)
    require(lseek(file, offsets[i], SEEK_SET) == offsets[i] &&
            write(file, "x", 1) == 1, "cannot write large file block");
  for(int i = 0; i < sizeof(offsets)/sizeof(*offsets); i++)
    require(lseek(file, offsets[i]-1, SEEK_SET) == offsets[i]-1 &&
            read(file, &data, 1) == 1 && !data &&
            read(file, &data, 1) == 1 && data == 'x', "large file block or hole corrupted");
  close(file);
  require(unlink(path) == 0, "cannot remove large file");
  printf(1, "DESKTOP INFRASTRUCTURE TESTS PASSED\n");
}

int
main(int argc, char **argv)
{
  if(argc == 2 && !strcmp(argv[1], "desktop")){
    desktop();
    return 0;
  }
  if(argc == 2 && !strcmp(argv[1], "smp")){
    smp();
    return 0;
  }
  if(argc == 3 && !strcmp(argv[1], "storage-guard")){
    storage_guard(argv[2]);
    return 0;
  }
  if(argc == 3 && !strcmp(argv[1], "storage-io")){
    storage_io(argv[2]);
    return 0;
  }
#ifndef P9_LEGACY_ROOT
  if(argc == 2 && !strcmp(argv[1], "inodes")){
    inode_pressure();
    return 0;
  }
#endif
  bad_arguments();
  memory_pressure();
  process_pressure();
  if(argc == 2 && !strcmp(argv[1], "hardening"))
    hardening();
  if(argc == 2 && !strcmp(argv[1], "full-disk"))
    disk_pressure();
  printf(1, "STABILITY TESTS PASSED\n");
  exitstatus(0);
}
