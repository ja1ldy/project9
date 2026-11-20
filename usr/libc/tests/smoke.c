/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static void
check(int ok, const char *msg)
{
  if(!ok){
    fprintf(stderr, "libctest: %s (%s)\n", msg, strerror(errno));
    exit(1);
  }
}

static volatile sig_atomic_t caught;
static jmp_buf jump;
static const char sample[] = "/tmp/libctest.data";

static void
handler(int sig)
{
  char buf[16];
  if(snprintf(buf, sizeof(buf), "%d", sig) > 0)
    caught = sig;
}

static void
strings(void)
{
  char buf[80], str[8], ch;
  unsigned u, x;
  int d;
  char *p = calloc(4, 9), *q;

  check(p && memcmp(p, "\0\0\0\0", 4) == 0, "calloc");
  memcpy(p, "abcdef", 7);
  memmove(p+1, p, 7);
  check(strcmp(p, "aabcdef") == 0, "overlapping memmove");
  q = realloc(p, 1000);
  check(q && !strcmp(q, "aabcdef"), "realloc");
  free(q);
  free(NULL);
  check(snprintf(buf, 4, "%04x", 15U) == 4 && !strcmp(buf, "000"), "snprintf truncation");
  check(snprintf(buf, sizeof(buf), "%-4s %d %u %#x %c", "ok", -42, 73U, 30U, 'X') == 18,
        "printf formats");
  check(sscanf(" -21 37 0x1f hi Z", "%d %u %x %7s %c", &d, &u, &x, str, &ch) == 5 &&
        d == -21 && u == 37 && x == 31 && !strcmp(str, "hi") && ch == 'Z', "sscanf");
  check(strtol("  -0x2e", NULL, 0) == -46 && fnmatch("a/[A-Z]*", "a/Ab", FNM_PATHNAME) == 0,
        "numeric and pattern parsing");
  check(setenv("P9_SDK_TEST", "yes", 1) == 0 && !strcmp(getenv("P9_SDK_TEST"), "yes") &&
        unsetenv("P9_SDK_TEST") == 0 && !getenv("P9_SDK_TEST"), "environment");
}

static void
files(void)
{
  FILE *f;
  struct stat st;
  DIR *dir;
  struct dirent *one;
  char line[8];

  unlink(sample);
  f = fopen(sample, "w+");
  check(f && fprintf(f, "ok\n") == 3 && ftell(f) == 3 &&
        fseek(f, 0, SEEK_SET) == 0, "stdio write and seek");
  check(fgets(line, sizeof(line), f) && !strcmp(line, "ok\n") &&
        fread(line, 1, 1, f) == 0 && feof(f) && fclose(f) == 0, "stdio read and eof");
  check(stat(sample, &st) == 0 && S_ISREG(st.st_mode) && st.st_size == 3, "file metadata");
  check(access(sample, R_OK) == 0, "file access");
  dir = opendir("/tmp");
  check(dir && (one = readdir(dir)) && !strcmp(one->d_name, "."), "read directory");
  rewinddir(dir);
  check((one = readdir(dir)) && !strcmp(one->d_name, ".") && closedir(dir) == 0, "rewind directory");
  check(unlink(sample) == 0, "unlink");
}

static void
private_maps(void)
{
  const int page = P9_PAGE_SIZE;
  int fd, child, status;
  char *rw, *ro;

  fd = open(sample, O_CREAT | O_EXCL | O_RDWR, 0600);
  check(fd >= 0 && write(fd, "file", 4) == 4, "prepare private mapping");
  rw = mmap(NULL, page, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  ro = mmap(NULL, 4, PROT_READ, MAP_PRIVATE, fd, 0);
  check(rw != MAP_FAILED && ro != MAP_FAILED && close(fd) == 0 &&
        !memcmp(rw, "file", 4) && !rw[4], "map file and zero fill its tail");
  rw[0] = 'F';
  child = fork();
  check(child >= 0, "fork with mapping");
  if(!child){
    if(rw[0] != 'F')
      _exit(1);
    rw[0] = 'G';
    _exit(0);
  }
  check(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
        WEXITSTATUS(status) == 0 && rw[0] == 'F' && ro[0] == 'f',
        "isolate writes to private mappings");
  fd = open("/dev/zero", O_RDONLY);
  check(fd >= 0 && read(fd, ro, 1) == -1 && read(fd, rw, 1) == 1 &&
        close(fd) == 0, "validate syscall buffers in private mappings");
  check(munmap(rw, page) == 0 && munmap(ro, 4) == 0 &&
        write(1, rw, 1) == -1 && unlink(sample) == 0, "unmap private pages");
}

static void
paths(void)
{
  int fd;
  char target[16], value, cwd[64];
  struct stat st;

  check(mkdir("/tmp/p9path", 0700) == 0 && mkdir("/tmp/p9path/sub", 0700) == 0,
        "create path fixtures");
  fd = open("/tmp/p9path/sub/data", O_CREAT | O_EXCL | O_RDWR, 0600U);
  check(fd >= 0 && write(fd, "x", 1) == 1 && fchmod(fd, 0640) == 0 &&
        fstat(fd, &st) == 0 && (st.st_mode & 0777) == 0640 && close(fd) == 0,
        "file mode and fchmod");
  check(symlink("sub", "/tmp/p9path/through") == 0 &&
        readlink("/tmp/p9path/through", target, sizeof(target)) == 3 &&
        memcmp(target, "sub", 3) == 0, "create and read relative symlink");
  check(symlink("/tmp/p9path/sub/data", "/tmp/p9path/absolute") == 0 &&
        stat("/tmp/p9path/absolute", &st) == 0 && st.st_size == 1, "absolute symlink");
  check((fd = open("/tmp/p9path/through/data", O_RDONLY)) >= 0 &&
        read(fd, &value, 1) == 1 && value == 'x' && close(fd) == 0, "follow symlink in path");
  check(symlink("cycle", "/tmp/p9path/cycle") == 0 &&
        open("/tmp/p9path/cycle", O_RDONLY) < 0, "bounded symlink traversal");
  check(rename("/tmp/p9path", "/tmp/p9path/sub/cycle") < 0, "reject rename into own subtree");
  check(rename("/tmp/p9path/sub", "/tmp/p9renamed") == 0 &&
        chdir("/tmp/p9renamed") == 0 && getcwd(cwd, sizeof(cwd)) &&
        !strcmp(cwd, "/tmp/p9renamed") && chdir("..") == 0 &&
        getcwd(cwd, sizeof(cwd)) && !strcmp(cwd, "/tmp"), "rename updates parent link");
  fd = open("p9path/replaced", O_CREAT | O_EXCL | O_WRONLY, 0600U);
  check(fd >= 0 && close(fd) == 0 && rename("p9renamed/data", "p9path/replaced") == 0 &&
        stat("p9path/replaced", &st) == 0 && st.st_size == 1, "atomic rename replacement");
  check(unlink("p9path/replaced") == 0 && unlink("p9path/through") == 0 &&
        unlink("p9path/absolute") == 0 && unlink("p9path/cycle") == 0 &&
        rmdir("p9renamed") == 0 && rmdir("p9path") == 0, "remove path fixtures");
}

static void
events(void)
{
  struct sigaction action = { .sa_handler = handler }, old;
  sigset_t mask;
  fd_set rd;
  struct timeval before, after, timeout = {0, 0};
  int fds[2], status, pid;
  char byte;
  volatile double a = 1.5, x = 0.25;

  check(gettimeofday(&before, NULL) == 0 && before.tv_sec > 946684800L, "rtc time");
  check(usleep(30000) == 0 && gettimeofday(&after, NULL) == 0 &&
        (after.tv_sec > before.tv_sec || after.tv_usec > before.tv_usec), "sleep and clock");
  check(sigaction(SIGINT, &action, &old) == 0 && old.sa_handler == SIG_DFL &&
        sigaction(SIGINT, NULL, &old) == 0 && old.sa_handler == handler, "sigaction query");
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  check(sigprocmask(SIG_BLOCK, &mask, NULL) == 0 && raise(SIGINT) == 0 && !caught &&
        sigprocmask(SIG_UNBLOCK, &mask, NULL) == 0 && caught == SIGINT, "signal mask");
  if(!setjmp(jump))
    longjmp(jump, 1);
  check(pipe(fds) == 0 && fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0 &&
        read(fds[0], &byte, 1) == -1 && errno == EAGAIN, "nonblocking pipe");
  FD_ZERO(&rd);
  FD_SET(fds[0], &rd);
  check(select(fds[0]+1, &rd, NULL, NULL, &timeout) == 0 && !FD_ISSET(fds[0], &rd), "poll empty pipe");
  pid = fork();
  check(pid >= 0, "fork");
  if(!pid){
    close(fds[0]);
    usleep(20000);
    _exit(write(fds[1], "k", 1) != 1);
  }
  close(fds[1]);
  FD_SET(fds[0], &rd);
  timeout = (struct timeval){1,0};
  check(select(fds[0]+1, &rd, NULL, NULL, &timeout) == 1 && FD_ISSET(fds[0], &rd) &&
        read(fds[0], &byte, 1) == 1 && byte == 'k', "select readable pipe");
  check(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && !WEXITSTATUS(status), "wait child");
  close(fds[0]);
  check(fabs(sin(x)*sin(x) + cos(x)*cos(x) - 1.0) < 1e-9 &&
        sqrt(9.0) == 3.0 && floor(-a) == -2.0 && ceil(-a) == -1.0 &&
        fabs(atan2(1.0, 0.0) - M_PI/2) < 1e-9, "x87 math");
}

static void
graphics(void)
{
  struct fbinfo info;
  struct input_event event;
  int mode, status, pid, fd, kbd;
  size_t size;
  volatile unsigned *pixels;

  kbd = open("/dev/keyboard", O_RDONLY | O_NONBLOCK);
  check(kbd >= 0 && ioctl(kbd, KBDIOGETMODE, &mode) == 0 && mode == KBD_MODE_SET1,
        "keyboard mode");
  while(read(kbd, &event, sizeof(event)) > 0)
    check(event.type == INPUT_KEY, "keyboard record");
  check(errno == EAGAIN, "nonblocking keyboard");
  close(kbd);
  fd = open("/dev/fb0", O_RDWR);
  check(fd >= 0 && ioctl(fd, FBIOGETINFO, &info) == 0 && info.bits_per_pixel == 32 &&
        info.width && info.height && info.stride >= info.width*4, "framebuffer geometry");
  size = info.stride * info.height;
  check(mmap(NULL, size+1, PROT_READ, MAP_SHARED, fd, 0) == MAP_FAILED && errno == EINVAL,
        "framebuffer size boundary");
  pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  check(pixels != MAP_FAILED && close(fd) == 0, "map framebuffer");
  pixels[0] = 0x00ff0099U;
  pid = fork();
  check(pid >= 0, "fork with framebuffer");
  if(!pid){
    int valid = pixels[0] == 0x00ff0099U;
    pixels[1] = 0x0000ff66U;
    _exit(!valid);
  }
  check(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && !WEXITSTATUS(status) &&
        pixels[1] == 0x0000ff66U, "shared pixels");
  check(munmap((void *)pixels, size) == 0, "unmap framebuffer");
}

static void
device_events(void)
{
  int kbd = open("/dev/keyboard", O_RDONLY | O_NONBLOCK);
  int mouse = open("/dev/mouse", O_RDONLY | O_NONBLOCK);
  int nfds = (kbd > mouse ? kbd : mouse) + 1;
  struct input_event event;
  int saw_key = 0, saw_pointer = 0;
  fd_set set;
  struct timeval tv = {3,0};

  check(kbd >= 0 && mouse >= 0, "open input devices");
  while(read(kbd, &event, sizeof(event)) > 0)
    ;
  while(read(mouse, &event, sizeof(event)) > 0)
    ;
  puts("INPUT WAIT");
  while(!saw_key || !saw_pointer){
    FD_ZERO(&set);
    if(!saw_key) FD_SET(kbd, &set);
    if(!saw_pointer) FD_SET(mouse, &set);
    check(select(nfds, &set, NULL, NULL, &tv) > 0, "select on input devices");
    if(FD_ISSET(kbd, &set) && read(kbd, &event, sizeof(event)) == sizeof(event))
      saw_key = event.type == INPUT_KEY;
    if(FD_ISSET(mouse, &set) && read(mouse, &event, sizeof(event)) == sizeof(event))
      saw_pointer = event.type == INPUT_RELATIVE && event.x == 13 && event.y == 7;
  }
  close(kbd);
  close(mouse);
  puts("INPUT TESTS PASSED");
}

int
main(int argc, char **argv)
{
  if(argc > 1 && strcmp(argv[1], "input") == 0){
    device_events();
    return 0;
  }
  strings();
  files();
  private_maps();
  paths();
  events();
  graphics();
  puts("LIBC TESTS PASSED");
  return 0;
}
