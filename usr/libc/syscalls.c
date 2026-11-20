/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define RAW(type, name, args) extern type raw_##name args __asm__(#name)

RAW(int, open, (const char *, int));
RAW(int, close, (int));
RAW(int, read, (int, void *, int));
RAW(int, write, (int, const void *, int));
RAW(int, lseek, (int, long, int));
RAW(int, unlink, (const char *));
RAW(int, link, (const char *, const char *));
RAW(int, unlinkdir, (const char *));
RAW(int, chdir, (const char *));
RAW(int, getcwd, (char *, int));
RAW(int, access, (const char *, int));
RAW(int, mkdir, (const char *));
RAW(int, chmod, (const char *, mode_t));
RAW(int, fchmod, (int, mode_t));
RAW(int, chown, (const char *, uid_t, gid_t));
RAW(int, umask, (int));
RAW(int, fork, (void));
RAW(int, execve, (const char *, char *const [], char *const []));
RAW(int, waitpid, (int, int *, int));
RAW(int, setsid, (void));
RAW(int, setuid, (uid_t));
RAW(int, setgid, (gid_t));
RAW(void *, sbrk, (ptrdiff_t));
RAW(int, pipe, (int *));
RAW(int, dup, (int));
RAW(int, dup2, (int, int));
RAW(int, ioctl, (int, int, void *));
RAW(int, fcntl, (int, int, int));
RAW(int, select, (int, fd_set *, fd_set *, fd_set *, struct timeval *));
RAW(int, mmap, (void *, size_t, int, int, int, off_t));
RAW(int, munmap, (void *, size_t));
RAW(int, gettimeofday, (struct timeval *, void *));
RAW(int, isatty, (int));

#include <project9/p9records.h>
RAW(int, statx, (const char *, struct p9_statx_record *));
RAW(int, fstatx, (int, struct p9_statx_record *));

int errno;

int
p9_result(int value, int legacy_error)
{
  if(value >= 0)
    return value;
  errno = value == -1 ? legacy_error : -value;
  return -1;
}

int
open(const char *path, int flags, ...)
{
  int fd, mask;
  mode_t mode;
  va_list ap;

  if(!(flags & O_CREAT))
    return p9_result(raw_open(path, flags), ENOENT);
  va_start(ap, flags);
  mode = va_arg(ap, mode_t);
  va_end(ap);
  mask = raw_umask(-1);
  raw_umask(mask | (0777 & ~mode));
  fd = raw_open(path, flags);
  raw_umask(mask);
  if(fd < 0 && (flags & O_EXCL) && raw_access(path, F_OK) == 0)
    return p9_result(-1, EEXIST);
  return p9_result(fd, EIO);
}

#define WRAP1(name, type, error) \
  int name(type x) { return p9_result(raw_##name(x), error); }

WRAP1(close, int, EBADF)
WRAP1(unlink, const char *, EACCES)
WRAP1(chdir, const char *, ENOENT)
WRAP1(setuid, uid_t, EPERM)
WRAP1(setgid, gid_t, EPERM)
WRAP1(pipe, int *, EMFILE)
WRAP1(dup, int, EBADF)

ssize_t
read(int fd, void *buf, size_t size)
{
  if(size > INT_MAX)
    return p9_result(-1, EINVAL);
  return p9_result(raw_read(fd, buf, size), EBADF);
}

ssize_t
write(int fd, const void *buf, size_t size)
{
  if(size > INT_MAX)
    return p9_result(-1, EINVAL);
  int rc = raw_write(fd, buf, size);
  if(rc == -EPIPE)
    raise(SIGPIPE);
  return p9_result(rc, EIO);
}

off_t lseek(int fd, off_t off, int how) { return p9_result(raw_lseek(fd, off, how), EINVAL); }
int link(const char *a, const char *b) { return p9_result(raw_link(a, b), EACCES); }
int rmdir(const char *path) { return p9_result(raw_unlinkdir(path), EACCES); }
int access(const char *path, int how) { return p9_result(raw_access(path, how), EPERM); }
int chmod(const char *path, mode_t mode) { return p9_result(raw_chmod(path, mode), EPERM); }
int fchmod(int fd, mode_t mode) { return p9_result(raw_fchmod(fd, mode), EPERM); }
int chown(const char *path, uid_t uid, gid_t gid) { return p9_result(raw_chown(path, uid, gid), EPERM); }
mode_t umask(mode_t m) { return raw_umask(m & 0777); }
pid_t fork(void) { return p9_result(raw_fork(), EAGAIN); }
pid_t setsid(void) { return p9_result(raw_setsid(), EPERM); }
pid_t waitpid(pid_t pid, int *st, int flags) { return p9_result(raw_waitpid(pid, st, flags), EINVAL); }
pid_t wait(int *status) { return waitpid(-1, status, 0); }
int execve(const char *path, char *const a[], char *const e[]) { return p9_result(raw_execve(path, a, e), ENOENT); }
int dup2(int old, int fd) { return p9_result(raw_dup2(old, fd), EBADF); }

char *
getcwd(char *buf, size_t size)
{
  if(!buf || size > INT_MAX || size < 2){
    errno = EINVAL;
    return NULL;
  }
  if(raw_getcwd(buf, size) < 0){
    errno = ERANGE;
    return NULL;
  }
  return buf;
}

void *
sbrk(ptrdiff_t n)
{
  void *p = raw_sbrk(n);
  if(p == (void *)-1)
    errno = ENOMEM;
  return p;
}

int
mkdir(const char *path, mode_t mode)
{
  int old = raw_umask(-1), rc;

  raw_umask(old | (0777 & ~mode));
  rc = raw_mkdir(path);
  raw_umask(old);
  return p9_result(rc, EACCES);
}

static int
statresult(int code, struct stat *st, struct p9_statx_record *i, int error)
{
  if(p9_result(code, error) < 0)
    return -1;
  *st = (struct stat){
    .st_dev = i->st.dev, .st_ino = i->st.ino, .st_nlink = i->st.nlink,
    .st_size = i->st.size, .st_blksize = 512, .st_uid = i->uid, .st_gid = i->gid,
    .st_mode = i->mode | (i->st.type == P9_T_DIR ? S_IFDIR : i->st.type == P9_T_FILE ? S_IFREG :
      i->st.type == P9_T_SOCKET ? S_IFSOCK : S_IFCHR)
  };
  return 0;
}

int
stat(const char *path, struct stat *st)
{
  struct p9_statx_record i;
  if(!st)
    return p9_result(-1, EFAULT);
  return statresult(raw_statx(path, &i), st, &i, ENOENT);
}

int
fstat(int fd, struct stat *st)
{
  struct p9_statx_record i;
  if(!st)
    return p9_result(-1, EFAULT);
  return statresult(raw_fstatx(fd, &i), st, &i, EBADF);
}

int
fcntl(int fd, int request, ...)
{
  va_list ap;
  int flags = 0;

  if(request == F_SETFL || request == F_SETFD){
    va_start(ap, request);
    flags = va_arg(ap, int);
    va_end(ap);
  }
  return p9_result(raw_fcntl(fd, request, flags), EINVAL);
}

int
ioctl(int fd, unsigned long request, ...)
{
  va_list ap;
  void *arg;

  switch(request){
  case TIOCSCTTY: return p9_result(raw_ioctl(fd, request, NULL), ENOTTY);
  case KBDIOGETMODE: case FBIOGETINFO: case TIOCGETA: case TIOCSETA:
  case TIOCSETAW: case TIOCSETAF: case TIOCGWINSZ: case TIOCSWINSZ: case TIOCGPTY:
  case FIONREAD:
    va_start(ap, request);
    arg = va_arg(ap, void *);
    va_end(ap);
    return p9_result(raw_ioctl(fd, request, arg), ENOTTY);
  default:
    return p9_result(-1, ENOTTY);
  }
}

int
isatty(int fd)
{
  if(raw_isatty(fd))
    return 1;
  errno = ENOTTY;
  return 0;
}

char *
ttyname(int fd)
{
  static char name[16];
  if(!isatty(fd))
    return NULL;
  int number;
  if(raw_ioctl(fd, TIOCGPTY, &number) == 0){
    strcpy(name, number & 1 ? "/dev/ptyp0" : "/dev/ttyp0");
    name[9] += number / 2;
  } else {
    strcpy(name, "/dev/console");
  }
  return name;
}

void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
  return (void *)p9_result(raw_mmap(addr, len, prot, flags, fd, off), EPERM);
}

int munmap(void *addr, size_t len) { return p9_result(raw_munmap(addr, len), EINVAL); }

int
select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv)
{
  return p9_result(raw_select(n, r, w, e, tv), EPERM);
}

int gettimeofday(struct timeval *t, void *z) { return p9_result(raw_gettimeofday(t, z), EIO); }

time_t
time(time_t *p)
{
  struct timeval now;
  time_t t = gettimeofday(&now, NULL) < 0 ? -1 : now.tv_sec;
  if(p)
    *p = t;
  return t;
}

int
nanosleep(const struct timespec *delay, struct timespec *rest)
{
  struct timeval limit, start, end;
  int have_time;
  long ns;

  if(!delay || delay->tv_sec < 0 || delay->tv_nsec < 0 ||
     delay->tv_nsec >= 1000000000)
    return p9_result(-1, EINVAL);
  limit.tv_sec = delay->tv_sec;
  limit.tv_usec = (delay->tv_nsec + 999) / 1000;
  if(limit.tv_usec == 1000000){
    if(limit.tv_sec == LONG_MAX)
      return p9_result(-1, EINVAL);
    limit.tv_sec++;
    limit.tv_usec = 0;
  }
  have_time = rest && gettimeofday(&start, NULL) == 0;
  if(select(0, NULL, NULL, NULL, &limit) == 0)
    return 0;
  if(rest && errno == EINTR){
    *rest = *delay;
    if(have_time && gettimeofday(&end, NULL) == 0){
      rest->tv_sec -= end.tv_sec - start.tv_sec;
      ns = rest->tv_nsec - (end.tv_usec - start.tv_usec)*1000;
      if(ns < 0){
        rest->tv_sec--;
        ns += 1000000000;
      } else if(ns >= 1000000000){
        rest->tv_sec++;
        ns -= 1000000000;
      }
      rest->tv_nsec = ns;
      if(rest->tv_sec < 0)
        *rest = (struct timespec){0,0};
    }
  }
  return -1;
}

int
usleep(unsigned us)
{
  struct timespec delay = { us / 1000000, (us % 1000000) * 1000 };
  return nanosleep(&delay, NULL);
}

unsigned
sleep(unsigned seconds)
{
  struct timespec delay = { seconds, 0 }, rest = delay;
  return nanosleep(&delay, &rest) == 0 ? 0 : (unsigned)rest.tv_sec + !!rest.tv_nsec;
}

RAW(int, rename, (const char *, const char *));
RAW(int, symlink, (const char *, const char *));
RAW(int, readlink, (const char *, char *, size_t));

int rename(const char *a, const char *b) { return p9_result(raw_rename(a, b), EIO); }
int symlink(const char *a, const char *b) { return p9_result(raw_symlink(a, b), EIO); }
ssize_t readlink(const char *path, char *buf, size_t n) { return p9_result(raw_readlink(path, buf, n), EIO); }

int getpagesize(void) { return P9_PAGE_SIZE; }

RAW(int, reboot, (int));
int reboot(int mode) { return p9_result(raw_reboot(mode), EPERM); }

#include <sys/socket.h>
RAW(int, socket, (int, int, int));
RAW(int, bind, (int, const struct sockaddr *, socklen_t));
RAW(int, listen, (int, int));
RAW(int, accept, (int, struct sockaddr *, socklen_t *));
RAW(int, connect, (int, const struct sockaddr *, socklen_t));

int socket(int d, int t, int p) { return p9_result(raw_socket(d, t, p), EINVAL); }
int bind(int f, const struct sockaddr *a, socklen_t n) { return p9_result(raw_bind(f, a, n), EINVAL); }
int listen(int f, int n) { return p9_result(raw_listen(f, n), EINVAL); }
int accept(int f, struct sockaddr *a, socklen_t *n) { return p9_result(raw_accept(f, a, n), EINVAL); }
int connect(int f, const struct sockaddr *a, socklen_t n) { return p9_result(raw_connect(f, a, n), EINVAL); }

#include <pty.h>
RAW(int, openpty, (int *));
int
openpty(int *master, int *slave, char *name, const struct termios *term,
        const struct winsize *size)
{
  int fds[2];
  if(!master || !slave || master == slave){
    errno = EINVAL;
    return -1;
  }
  int number = p9_result(raw_openpty(fds), EIO);
  if(number < 0)
    return -1;
  if((term && tcsetattr(fds[1], TCSANOW, term) < 0) ||
     (size && ioctl(fds[1], TIOCSWINSZ, size) < 0)){
    int saved = errno;
    close(fds[0]);
    close(fds[1]);
    errno = saved;
    return -1;
  }
  *master = fds[0];
  *slave = fds[1];
  if(name){
    strcpy(name, "/dev/ttyp0");
    name[9] = '0' + number;
  }
  return 0;
}

#include <poll.h>
RAW(int, poll, (struct pollfd *, nfds_t, int));
int poll(struct pollfd *fds, nfds_t n, int ms) { return p9_result(raw_poll(fds, n, ms), EINVAL); }
RAW(int, shutdown, (int, int));
int shutdown(int fd, int how) { return p9_result(raw_shutdown(fd, how), EINVAL); }

RAW(int, filemtime, (const char *, time_t *));
int filemtime(const char *p, time_t *time) { return p9_result(raw_filemtime(p, time), EINVAL); }

int
execvp(const char *name, char *const argv[])
{
  if(!name || !*name){
    errno = ENOENT;
    return -1;
  }
  if(strchr(name, '/'))
    return execve(name, argv, environ);

  const char *path = getenv("PATH");
  if(!path)
    path = "/bin:/usr/bin:/usr/X11/bin";
  char file[PATH_MAX];
  size_t length = strlen(name);
  int found = 0;
  do {
    const char *end = strchr(path, ':');
    size_t n = end ? (size_t)(end-path) : strlen(path);
    if(n + length + 1 + !!n <= sizeof(file)){
      if(n){
        memcpy(file, path, n);
        file[n++] = '/';
      }
      memcpy(file+n, name, length+1);
      execve(file, argv, environ);
      found |= access(file, F_OK) == 0;
    }
    path = end ? end+1 : NULL;
  } while(path);
  errno = found ? EACCES : ENOENT;
  return -1;
}
